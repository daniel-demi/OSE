#!/usr/bin/python

import os
import sys

args = ["-h", "--help", "--git-to-lab", "--lab-to-git", "-g", "-l"]

home_dir = '/home/daniel'
user_repo = os.path.join(home_dir,"OSE-git")
course_repo = os.path.join(home_dir,"lab")

def sync_dir(src, dst):
    for f in os.listdir(src):
        os.system("mkdir -p " + dst)
        d_path = os.path.join(dst, f)
        s_path = os.path.join(src, f)
        if os.path.isfile(s_path):
            os.system("cp " + s_path + " " + d_path)
            continue
        if os.path.isdir(s_path):
            sync_dir(s_path, d_path)

def main(src,dst):
    for f in os.listdir(src):
        if f.startswith('.'):
            continue
        d_path = os.path.join(dst, f)
        s_path = os.path.join(src, f)
        if os.path.isdir(s_path):
            if f == 'obj':
                continue
            sync_dir(s_path, d_path)
            continue
        if os.path.isfile(s_path):
            os.system("cp " + s_path + " " + d_path)
    print "Done!"

def print_help():
    print
    print "-h,--help        info"
    print "-g,--git-to-lab  sync lab with OSE-git"
    print "-l,--lab-to-git  sync OSE-git with lab"


if __name__ == '__main__':
    argv = sys.argv
    if len(argv) > 2:
        print "Error: too many arguments"
        print_help()
        exit(1) 
    if len(argv) == 1:
        print "Error: no arguments were given"
        print_help()
        exit(1)
    for a in argv[1:]:
        if a not in args:
            print "Error: invalid argument \"" + a + "\""
            print_help()
            exit(1)
    if argv[1] == "-h" or argv[1] == "--help":
        print "-h,--help        info"
        print "-g,--git-to-lab  sync lab with OSE-git"
        print "-l,--lab-to-git  sync OSE-git with lab"
        exit(0)
    if argv[1] == "-g" or argv[1] == "--git-to-lab":
        if raw_input("Copying files from OSE-git to lab. Continue? (y/N) ").lower()\
             == 'y':
            main(user_repo, course_repo)
        else:
            print "Aborted"
        exit()
    if argv[1] == "-l" or argv[1] == "--lab-to-git":
        if raw_input("Copying files from lab to OSE-git. Continue? (y/N) ").lower()\
             == 'y':
            main(course_repo, user_repo)
        else:
            print "Aborted"
        exit()

