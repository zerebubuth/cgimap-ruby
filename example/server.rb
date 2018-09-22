require 'rack'
$:.unshift(File.expand_path('../lib'))
require 'cgimap'

class Request
  attr_reader :env
  def initialize(env)
    @env = env
  end
end

class Server
  def initialize
    @rl = Cgimap::RateLimiter.new(
      memcache: 'memcached',
      ratelimit: 204_800,
      maxdebt: 250
    )
    @rt = Cgimap::Routes.new
    @f = Cgimap.create_backend('backend' => 'staticxml', 'file' => 'empty.osm')

    # you can create a backend which connects to a database as well. just
    # comment out the staticxml example above and use something like the
    # following:
    #
    # @f = Cgimap.create_backend(
    #   backend:   'apidb',
    #   cachesize: 100_000,
    #   dbname:    'openstreetmap',
    #   pidfile:   '/dev/null',
    #   logfile:   '/dev/stdout'
    # )
    #
    # other useful options that you might need are: host, username, password,
    # charset. when host isn't specified, it'll connect to PostgreSQL via a
    # local UNIX domain socket.
  end

  def call(env)
    Cgimap.process_request(Request.new(env), @rl, 'Magic cgimap-ruby', @rt, @f)
  end
end

Rack::Handler::WEBrick.run(Server.new, Port: 8000)
