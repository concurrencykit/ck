#!/bin/bash

{
  ## setup
  ./configure
  ## ensure compatibility
  make regressions
  ## build
  make
  ## install
  make install
}
