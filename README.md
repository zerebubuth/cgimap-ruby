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

    bundle exec rake compile

Or, if you installed cgimap in `$CGIMAP_DIR`:

    bundle exec rake compile -- --with-cgimap-include=$CGIMAP_DIR/include \
	  --with-cgimap-lib=$CGIMAP_DIR/lib

If you haven't installed cgimap, and just have compiled sources in a checked-out directory:

    bundle exec rake compile -- --with-cgimap-include=$CGIMAP_DIR/include \
	  --with-cgimap-lib=$CGIMAP_DIR/src/.libs

I haven't yet got to the point of being able to install this as a gem, but you can play with it in Ruby by running this in `irb`:

    $:.unshift(File.expand('./lib'))
	require 'cgimap'

And you can run the tests with:

    bundle exec rake test

## Example server

As a toy or example, and useful while developing, there's an example server in the `example/` directory. This starts up WEBrick and uses CGImap to serve requests. For example, to start the server:

    cd example
	bundle exec ruby server.rb

And, in another terminal:

    curl -v http://localhost:8000/api/0.6/map?bbox=0,0,0,0

Or other API calls. These should return the standard set of headers and XML or JSON documents.
