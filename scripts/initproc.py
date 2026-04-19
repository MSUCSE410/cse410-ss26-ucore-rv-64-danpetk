import os
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('INIT_PROC', default="usershell")
    args = parser.parse_args()
    f = open("os/initproc.S", mode="w")
    f.write(
'''
    .global INIT_PROC_NAME
INIT_PROC_NAME:
    .string \"{0}\"
'''.format(args.INIT_PROC));