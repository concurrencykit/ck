#!/bin/sh

set -e

ln -sf build/debian debian

dpkg-buildpackage -rfakeroot -tc

rm debian
