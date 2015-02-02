### Building for Debian/Ubuntu

This is a step by step guide to build Concurrency-Kit packages for Debian or Ubuntu. All of the steps
have been tested on recent Ubuntu (14.04) and Debian 7.

  # /bin/bash
  # sudo su -
  # apt-get -y install build-essential checkinstall git-core
  # git clone https://github.com/concurrencykit/ck.git
  # cd ck
  # build/build-debian.sh
  
These steps will create four output files they will be found in the lower directory folder 

  # cd ..
  # ls -1
  ck_0.4.5-1_amd64.changes
  ck_0.4.5-1_amd64.deb
  ck_0.4.5-1.dsc
  ck_0.4.5-1.tar.gz
  
To install the package where 0.4.5 is the version that you have just built.

  # dpkg -i ck_0.4.5-1_amd.deb
  
