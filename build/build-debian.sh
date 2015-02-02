#!/bin/sh
# A simple install script that will automate the build of a debian/ubuntu
# patckage for Concurrency-Kit.

set -e

# get the dependencies outlined in the control file
sudo apt-get -y install debhelper build-essential
 
# create a symbolic link so that the dpkg-buildpackage command will find the rules file
ln -sf build/debian debian

# build the package from source using fakeroot to match paths. the output packages will
# be in the folder above the package root. i.e. "cd .."
dpkg-buildpackage -rfakeroot -tc

# delete the debian symbolic link created above.
rm debian
