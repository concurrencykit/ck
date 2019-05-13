### Testing

| Environment | Status |
| -------- | ------ |
| Drone | [![Build Status](https://cloud.drone.io/api/badges/concurrencykit/ck/status.svg)](https://cloud.drone.io/concurrencykit/ck)
| Travis | [![Build Status](https://travis-ci.org/concurrencykit/ck.svg)](https://travis-ci.org/concurrencykit/ck)
| Cirrus | [![Build Status](https://api.cirrus-ci.com/github/concurrencykit/ck.svg)](https://cirrus-ci.com/github/concurrencykit/ck)

### Build

* Step 1.  
        `./configure`  
        For additional options try `./configure --help`  

* Step 2.  
        In order to compile regressions (requires POSIX threads) use  
        `make regressions`. In order to compile libck use `make all` or `make`.  

* Step 3.  
	In order to install use `make install`  
	To uninstall use `make uninstall`.  

See http://concurrencykit.org/ for more information.
