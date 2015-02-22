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
    @rl = Cgimap::RateLimiter.new({})
    @rt = Cgimap::Routes.new
    @f = Cgimap::create_backend('backend' => 'staticxml', 'file' => 'empty.osm')
  end

  def call(env)
    Cgimap::process_request(Request.new(env), @rl, "foo", @rt, @f)
  end
end

Rack::Handler::WEBrick.run(Server.new, :Port => 8000)
