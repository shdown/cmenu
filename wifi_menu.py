#!/usr/bin/env python3
import subprocess
import sys
import os
import dbus
import select
import signal
from contextlib import contextmanager


CMENU = [
    '/home/v/repo/cmenu/cpp/cmenu',
    # your -style-{header,hi,entry}=... arguments here
]


GAUGE_BRIGHT, GAUGE_DIM = '●', '○'


class IwdDevice:
    def __init__(self, path, name):
        self.path = path
        self.name = name


class IwdNetwork:
    def __init__(self, path, name, dbm, type_str, connected):
        self.path = path
        self.name = name
        self.dbm = dbm
        self.type_str = type_str
        self.connected = connected


class IwdConnection:
    def __init__(self):
        self._bus = dbus.SystemBus()

    def _fetch_managed_objects(self):
        mgr_iface = dbus.Interface(
            self._bus.get_object('net.connman.iwd', '/'),
            'org.freedesktop.DBus.ObjectManager')
        return dict(mgr_iface.GetManagedObjects())

    def fetch_devices(self):
        objs = self._fetch_managed_objects()
        for k, v in objs.items():
            dev = v.get('net.connman.iwd.Device')
            if dev is not None:
                yield IwdDevice(
                    path=str(k),
                    name=str(dev['Name']))

    # returns a bool indicating if this is a new scan
    def scan(self, device):
        station_iface = dbus.Interface(
            self._bus.get_object('net.connman.iwd', device.path),
            'net.connman.iwd.Station')

        try:
            station_iface.Scan()
            return True
        except dbus.exceptions.DBusException as ex:
            name = ex.get_dbus_name()
            if name == 'net.connman.iwd.InProgress':
                return False
            raise

    def fetch_networks(self, device):
        station_iface = dbus.Interface(
            self._bus.get_object('net.connman.iwd', device.path),
            'net.connman.iwd.Station')

        networks = station_iface.GetOrderedNetworks()

        objs = self._fetch_managed_objects()

        for path, rssi in networks:
            obj = objs.get(path)
            if obj is not None:
                net = obj['net.connman.iwd.Network']
                yield IwdNetwork(
                    path=str(path),
                    name=str(net['Name']),
                    dbm=int(rssi) / 100,
                    type_str=str(net['Type']),
                    connected=bool(net['Connected']))


class FileDescriptorWrapper:
    def __init__(self, fd=-1):
        self.fd = fd

    def release(self):
        result = self.fd
        self.fd = -1
        return result

    def close(self):
        fd = self.release()
        if fd != -1:
            os.close(fd)

    def move_out_to_new_copy(self):
        return FileDescriptorWrapper(self.release())

    def __enter__(self):
        return self

    def __exit__(self, _exc_type, _exc_value, _exc_tb):
        self.close()

    def __del__(self):
        self.close()


class ChildWrapper:
    def __init__(self, pid=-1):
        self.pid = pid

    def wait(self):
        if self.pid > 0:
            _, status = os.waitpid(self.pid, 0)
            self.pid = -1
            return os.waitstatus_to_exitcode(status)
        return None


def spawn_child(argv):
    pid = os.spawnvp(os.P_NOWAIT, argv[0], argv)
    return ChildWrapper(pid)


@contextmanager
def pipe():
    raw_fds = os.pipe()
    fds = FileDescriptorWrapper(raw_fds[0]), FileDescriptorWrapper(raw_fds[1])
    try:
        yield fds
    finally:
        fds[0].close()
        fds[1].close()


@contextmanager
def wait_for_child(child, check_exitcode=True):
    try:
        yield None
    finally:
        code = child.wait()
        if check_exitcode and code != 0:
            raise ValueError(f'child exited with code {code}')


def poll_for_input(fd, timeout):
    ready_fds, _, _ = select.select([fd.fd], [], [], timeout)
    return bool(ready_fds)


def read_line_from_fd(fd, check_whole_line=False):
    chunks = []
    while True:
        b = os.read(fd.fd, 1)
        chunks.append(b)
        if (not b) or (b == b'\n'):
            break

    line_bytes = b''.join(chunks)

    if check_whole_line and not line_bytes.endswith(b'\n'):
        raise ValueError('expected to read a whole line')

    return line_bytes.decode()


def launch_cmenu(args):
    with pipe() as pipe1, pipe() as pipe2:
        their_in, my_out = pipe1
        my_in, their_out = pipe2

        os.set_inheritable(their_in.fd, True)
        os.set_inheritable(their_out.fd, True)

        child = spawn_child([*CMENU, f'-infd={their_in.fd}', f'-outfd={their_out.fd}', *args])

        return (
            child,
            my_in.move_out_to_new_copy(),
            os.fdopen(my_out.release(), 'w'),
        )


class NetworkList:
    def __init__(self):
        self._nets = []
        self._idx_by_path = {}

    def add_and_get_delta(self, new_nets):
        new_paths = frozenset(net.path for net in new_nets)
        delta = []

        for idx, net in enumerate(self._nets):
            if net.path not in new_paths:
                delta.append(('=', idx, net, False))

        for net in new_nets:
            idx = self._idx_by_path.get(net.path)
            if idx is not None:
                delta.append(('=', idx, net, True))
            else:
                self._idx_by_path[net.path] = len(self._nets)
                self._nets.append(net)
                delta.append(('+', None, net, True))

        return delta

    def get_by_index(self, idx):
        return self._nets[idx]


def escape_str(s):
    return ''.join(filter(lambda c: c.isprintable(), s))


def make_gauge(dbm, size):
    MAX_DBM = -20
    MIN_DBM = -90

    level = 1 - 0.7 * (MAX_DBM - dbm) / (MAX_DBM - MIN_DBM)

    level = max(level, 0.0)
    level = min(level, 1.0)

    nbright = round(level * size)

    return (GAUGE_BRIGHT * nbright) + (GAUGE_DIM * (size - nbright))


def net_to_columns(net, is_available):
    if is_available:
        status = '(*)' if net.connected else ''
    else:
        status = '---'
    return (
        status,
        net.type_str,
        make_gauge(net.dbm, size=5),
        escape_str(net.name),
    )


def device_list_dialog(devices):
    child, in_fd, out_f = launch_cmenu([
        '-column=:Device',
    ])
    with wait_for_child(child), in_fd, out_f:
        try:
            out_f.write(f'n {len(devices)}\n')
            for device in devices:
                out_f.write('+\n')
                out_f.write(escape_str(device.name))
                out_f.write('\n')
            out_f.flush()
        except OSError:
            return None

        line = read_line_from_fd(in_fd)
        if line != 'ok\n':
            if line == '':
                return None
            raise ValueError(f'unexpected line: "{line}"')

        line = read_line_from_fd(in_fd)
        if line == '':
            return None
        if line == 'result\n':
            line = read_line_from_fd(in_fd, check_whole_line=True)
            index = int(line.rstrip('\n'))
            return devices[index]
        raise ValueError(f'unexpected line: "{line}"')


def network_list_dialog(conn, device):
    child, in_fd, out_f = launch_cmenu([
        '-command=r:Rescan',
        '-command=d:Disconnect',
        '-column=@7:Status',
        '-column=@5:Type',
        '-column=@7:Signal',
        '-column=:Name',
    ])

    with wait_for_child(child), in_fd, out_f:
        network_list = NetworkList()
        line = None
        while True:
            new_nets = list(conn.fetch_networks(device))
            delta = network_list.add_and_get_delta(new_nets)
            try:
                out_f.write(f'n {len(delta)}\n')
                for command, index, net, is_available in delta:
                    if index is not None:
                        out_f.write(f'{command} {index}\n')
                    else:
                        out_f.write(f'{command}\n')
                    for column in net_to_columns(net, is_available):
                        out_f.write(column)
                        out_f.write('\n')
                out_f.flush()
            except OSError:
                line = None
                break

            line = read_line_from_fd(in_fd)
            if line != 'ok\n':
                break

            if poll_for_input(in_fd, 1.0):
                line = None
                break

        if line is None:
            line = read_line_from_fd(in_fd)

        if line == '':
            return None, '_quit'
        if line == 'result\n':
            line = read_line_from_fd(in_fd, check_whole_line=True)
            index = int(line.rstrip('\n'))
            return network_list.get_by_index(index), None
        if line == 'custom\n':
            line = read_line_from_fd(in_fd, check_whole_line=True)
            return None, line.rstrip('\n')
        raise ValueError(f'unexpected line: "{line}"')


def sighandler(_signo, _frame):
    os._exit(0)


def actual_main():
    conn = IwdConnection()
    devices = list(conn.fetch_devices())
    if not devices:
        print('No devices found.')
        input('Press Enter to continue >>> ')
        sys.exit(0)
    if len(devices) == 1:
        device = devices[0]
    else:
        device = device_list_dialog(devices)
        if device is None:
            sys.exit(0)

    while True:
        conn.scan(device)
        net, alt = network_list_dialog(conn, device)
        if net is not None:
            while True:
                p = subprocess.run([
                    'iwctl',
                    'station',
                    device.name,
                    'connect',
                    net.name,
                    net.type_str,
                ])
                if p.returncode == 0:
                    break
                input('Retry? (Enter/Ctrl+C) >>> ')
            break
        elif alt == '_quit':
            sys.exit(0)
        elif alt == 'r':
            continue
        elif alt == 'd':
            p = subprocess.run([
                'iwctl',
                'station',
                device.name,
                'disconnect',
            ])
            if p.returncode != 0:
                input('Press Enter to continue >>> ')
            break
        else:
            raise ValueError(f'unexpected alt "{alt}"')


def main():
    signal.signal(signal.SIGINT, sighandler)
    signal.signal(signal.SIGTERM, sighandler)

    try:
        actual_main()
    except dbus.exceptions.DBusException as ex:
        name = ex.get_dbus_name()
        message = str(ex)
        print(f'D-Bus error: {name}: {message}')
        input('Press Enter to continue >>> ')


if __name__ == '__main__':
    main()
