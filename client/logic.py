import socket
import random
import sys
import re
import os

'''

Acceptable command:

user, pass, syst, type, quit, exit(=abor)
passive, passive on, passive off
get, put, list <-param>
pwd, cd, mkdir, rmdir

'''


class ClientLogic:

    RECEIVE_DATA_MAX = 8192

    COMMAND_DIRECTLY_NO_PARAM = {
        'syst': 'syst',
        'quit': 'quit',
        'exit': 'abor',
        'pwd': 'pwd',
    }
    COMMAND_DIRECTLY_ONE_PARAM = {
        'user': 'user',
        'pass': 'pass',
        'type': 'type',
        'cd': 'cwd',
        'mkdir': 'mkd',
        'rmdir': 'rmd',
    }

    def __init__(self):
        try:
            print('Connected to ' + sys.argv[1] + '...')
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((sys.argv[1], int(sys.argv[2])))
            self.isPassive = True

        except:
            print('Connection refused.')
            sys.exit(1)

        code = self.receive_code()
        print(code)

    def extract(self, st):
        return st.split('/')[-1]

    def send_code(self, code):
        self.socket.send((code.strip() + '\r\n').encode('utf-8'))

    def receive_code(self):
        return self.socket.recv(self.RECEIVE_DATA_MAX).decode().strip()

    def send_data(self, trans_socket: socket.socket, filefd):
        trans_socket.sendall(filefd.read())
        filefd.close()
        trans_socket.close()

    def receive_data(self, trans_socket: socket.socket, filefd):
        while True:
            recv_data = trans_socket.recv(self.RECEIVE_DATA_MAX)
            if not filefd:
                recv_data = recv_data.decode()
                print(recv_data, end='')
            else:
                filefd.write(recv_data)
            if not recv_data:
                break
        if filefd:
            filefd.close()
        trans_socket.close()

    def send_passive(self, verb, param):
        self.send_code('pasv')
        recv_code = self.receive_code()
        print(recv_code)
        ip_port_list = re.findall('\((\d+,\d+,\d+,\d+,\d+,\d+)\)', recv_code)[0].split(',')
        ip = '.'.join(ip_port_list[:4])
        port = int(ip_port_list[4]) * 256 + int(ip_port_list[5])
        trans_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        trans_socket.connect((ip, port))
        self.send_code(verb + ' ' + param)
        return trans_socket, self.receive_code()

    def send_active(self, verb, param):
        ip = self.socket.getsockname()[0]
        ip_comma = ','.join(ip.split('.'))
        conn_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        while True:
            port = random.randint(20000, 65535)
            flag = True
            try:
                conn_socket.bind((ip, port))
            except:
                flag = False

            if flag:
                break
        conn_socket.listen(1)
        self.send_code('port ' + ip_comma + ',' + str(port // 256) + ',' + str(port % 256))
        print(self.receive_code())

        self.send_code(verb + ' ' + param)
        return conn_socket, self.receive_code()

    def get_passive_mode(self):
        if self.isPassive:
            return 'Passive mode: on.'
        else:
            return 'Passive mode: off.'

    def login(self):
        self.send_code('USER ' + input('Name: '))
        code = self.receive_code()
        print(code)
        if code.startswith('331'):
            self.send_code('PASS ' + input('Password: '))
            print(self.receive_code())

    def execute(self):
        while True:
            code = input('ftp> ').strip()
            codes = code.split()

            if not codes:
                continue

            codes[0] = codes[0].lower()
            if codes[0] in self.COMMAND_DIRECTLY_NO_PARAM:
                self.send_code(self.COMMAND_DIRECTLY_NO_PARAM[codes[0]])
                recv_code = self.receive_code()
                print(recv_code)
                if recv_code.startswith('221'):
                    sys.exit(0)

            elif codes[0] in self.COMMAND_DIRECTLY_ONE_PARAM:
                if len(codes) < 2:
                    codes.append('')

                self.send_code(self.COMMAND_DIRECTLY_ONE_PARAM[codes[0]] + ' ' + codes[1])
                print(self.receive_code())

            elif codes[0] == 'passive':
                if len(codes) < 2:
                    self.isPassive = not self.isPassive
                    print(self.get_passive_mode())
                else:
                    if codes[1] == 'on':
                        self.isPassive = True
                        print(self.get_passive_mode())
                    elif codes[1] == 'off':
                        self.isPassive = False
                        print(self.get_passive_mode())
                    else:
                        print('usage: passive [on | off]')

            elif codes[0] == 'get':
                if len(codes) < 2:
                    print('?Invalid command.')
                    continue

                if self.isPassive:
                    trans_socket, recv_code = self.send_passive('retr', codes[1])
                    print(recv_code)

                    if not recv_code.startswith('150'):
                        trans_socket.close()
                        continue

                    f = open(self.extract(codes[1]), 'wb')
                    self.receive_data(trans_socket, f)
                    print(self.receive_code())

                else:
                    conn_socket, recv_code = self.send_active('retr', codes[1])
                    print(recv_code)

                    if not recv_code.startswith('150'):
                        conn_socket.close()
                        break

                    trans_socket, addr = conn_socket.accept()

                    f = open(self.extract(codes[1]), 'wb')
                    self.receive_data(trans_socket, f)
                    conn_socket.close()
                    print(self.receive_code())

            elif codes[0] == 'put':
                if len(codes) < 2:
                    print('?Invalid command.')
                    continue

                if not os.path.exists(codes[1]):
                    print('ftp: Can\'t open \'' + codes[1] + '\': No such file or directory')
                    continue

                if self.isPassive:
                    trans_socket, recv_code = self.send_passive('stor', codes[1])
                    print(recv_code)

                    if not recv_code.startswith('150'):
                        trans_socket.close()
                        break

                    f = open(self.extract(codes[1]), 'rb')
                    self.send_data(trans_socket, f)
                    print(self.receive_code())

                else:
                    conn_socket, recv_code = self.send_active('stor', codes[1])
                    print(recv_code)

                    if not recv_code.startswith('150'):
                        conn_socket.close()
                        break

                    trans_socket, addr = conn_socket.accept()

                    f = open(self.extract(codes[1]), 'rb')
                    self.send_data(trans_socket, f)
                    conn_socket.close()
                    print(self.receive_code())

            elif codes[0] == 'ls':
                if len(codes) < 2:
                    codes.append('')

                if self.isPassive:
                    trans_socket, recv_code = self.send_passive('list', codes[1])
                    print(recv_code)

                    if not recv_code.startswith('150'):
                        trans_socket.close()
                        break

                    self.receive_data(trans_socket, None)
                    print(self.receive_code())

                else:
                    conn_socket, recv_code = self.send_active('list', codes[1])
                    print(recv_code)

                    if not recv_code.startswith('150'):
                        conn_socket.close()
                        break

                    trans_socket, addr = conn_socket.accept()

                    self.receive_data(trans_socket, None)
                    conn_socket.close()
                    print(self.receive_code())
            else:
                print('?Invalid command.')

    def start(self):
        self.login()
        self.execute()
