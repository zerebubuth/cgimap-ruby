# cgimap-ruby #

A gem exposing cgimap functionality to Ruby.

## Warning ##

This is still very much work in progress, and probably doesn't even work. Please proceed with caution and [report any issues](https://github.com/zerebubuth/cgimap-ruby/issues/new) you find.

## Installation ##

First, you will need [cgimap](https://github.com/zerebubuth/openstreetmap-cgimap)'s [`library_split` branch](https://github.com/zerebubuth/openstreetmap-cgimap/tree/library_split). The README there should explain installing it, and it's basically just your usual `./configure && make && make install` GNU autotools thing.

Next, you will need Ruby. I prefer to use [rbenv](http://rbenv.org/) to manage my (user) Ruby installation, and I have not tested this installation with a system-installed Ruby. You'll also need [Bundler](http://bundler.io/). If you don't already have this then you can probably install it by running:

    gem install bundler

If that doesn't work, please check the Bundler instructions for your OS. You should then be able to install the necessary gems with:

    bundle install

Which might take some time. At the end of that, you should be able to compile the gem:

    rake compile

Or, if you installed cgimap in `$CGIMAP_DIR`:

    rake compile -- --with-cgimap-include=$CGIMAP_DIR/include \
	  --with-cgimap-lib=$CGIMAP_DIR/lib

I haven't yet got to the point of being able to install this as a gem, but you can play with it in Ruby by running this in `irb`:

    $:.unshift(File.expand('./lib'))
	require 'cgimap'

