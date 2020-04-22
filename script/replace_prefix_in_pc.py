#!/usr/bin/python3

import sys, getopt
import os.path
from os import path

def replaceByFile(filename, prefix):
    if not path.isfile(filename):
        print('File does not exist')
        sys.exit(2)
    writeData = ''
    with open(filename) as f:
        for line in f:
            if line.find('prefix') == 0:
                writeData += ('prefix='+str(prefix)+'\n')
            else:
                writeData += line
    
    with open(filename, 'w') as f:
        f.write(writeData)
    
    

def replaceByDir(dirname, prefix):
    if not path.isdir(dirname):
        print('Directory does not exist')
        sys.exit(2)
    for filename in os.listdir(dirname):
        if filename.endswith('.pc'):
            replaceByFile(dirname+'/'+filename, prefix)

def main(argv):
    inputfile = ''
    inputdir = ''
    prefix = ''
    try:
        opts, args = getopt.getopt(argv,"hi:d:p:",["inputfile=","inputdir=", "prefix=","help"])
    except getopt.GetoptError:
        print('usage: replace_prefix_in_pc.py (-i <inputfile> || -d <inputdir>) -p <prefix> [-h]')
        sys.exit(2)
    for opt, arg in opts:
        if opt in ('-h', "--help"):
            print('usage: replace_prefix_in_pc.py (-i <inputfile> || -d <inputdir>) -p <prefix> [-h]')
            sys.exit()
        elif opt in ("-i", "--inputfile"):
            inputfile = arg
        elif opt in ("-d", "--inputdir"):
            inputdir = arg
        elif opt in ("-p", "--prefix"):
            prefix = arg
    if len(prefix) == 0:
        print('No prefix specified')
        print('usage: replace_prefix_in_pc.py (-i <inputfile> || -d <inputdir>) -p <prefix> [-h]')
        sys.exit(2)
    if len(inputfile) != 0:
        replaceByFile(inputfile, prefix)
    elif len(inputdir) != 0:
        replaceByDir(inputdir, prefix)
    else:
        print('No input file specified')
        print('usage: replace_prefix_in_pc.py (-i <inputfile> || -d <inputdir>) -p <prefix> [-h]')
        sys.exit(2) 

if __name__ == "__main__":
   main(sys.argv[1:])