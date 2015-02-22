require 'minitest/autorun'
require 'rack'
require 'stringio'
require 'net/http'
require 'net/http/response'
require 'rexml/document'
require 'set'

$:.unshift(File.expand_path('./lib'))
require 'cgimap'

##
# uses (some of) the upstream cgimap tests to make
# sure that the module is working properly.
#
# the tests are set up as directories, each with a
# 'data.osm' XML file in it, plus various '*.case'
# files which define input and output headers plus
# a body to compare against.
#
# this class basically just re-implements the rules
# in the upstream cgimap tests.
#
class CgimapUpstreamTest < Minitest::Unit::TestCase
  def compare_content_recursive_xml(a, b)
    return if a.text == '***'

    # check attributes
    keys_a = Set.new(a.attributes.map {|k, v| k})
    keys_b = Set.new(b.attributes.map {|k, v| k})

    for k in keys_a
      unless a.attribute(k).value == '***'
        assert_equal a.attribute(k), b.attribute(k), "Attribute values"
      end
    end

    # check children
    elems_a = a.elements.select {|c| c.node_type == :element}
    elems_b = b.elements.select {|c| c.node_type == :element}

    assert_equal elems_a.length, elems_b.length, "Number of children\nA: #{elems_a.inspect}\nB: #{elems_b.inspect}"
    elems_a.zip(elems_b).each do |child_a, child_b|
      compare_content_recursive_xml(child_a, child_b)
    end
  end

  def compare_content_xml(a, b)
    doc_a = REXML::Document.new a
    doc_b = REXML::Document.new b

    compare_content_recursive_xml(doc_a.root, doc_b.root)
  end

  def compare_content_json(a, b)
    # TODO!
    flunk("unimplemented")
  end

  def run_testcore(file, use_hijack)
    dir = File.dirname(file)
    osm_data = File.join(dir, "data.osm")

    # read the test case definition from the file - it's
    # three sections delimited by lines containing only
    # "---".
    req_hdrs, resp_hdrs, resp_body = File.readlines(file).
      chunk { |l| l == "---\n" }.
      select {|p,l| not p}.
      map {|p,l| l}

    # normalise headers and re-join the body lines back
    # together because we want to compare that exactly.
    request_headers = Hash[req_hdrs.map {|l| k,v = l.chop.split(/: */); [k.upcase.gsub(/-/,'_'), v]}]
    response_headers = Hash[resp_hdrs.map {|l| k,v = l.chop.split(/: */); [k.upcase, v]}]
    body = resp_body.nil? ? "" : resp_body.join

    # parse out a couple of special fields which need to
    # be treated differently in Rack.
    method = "GET"
    if request_headers.include?('REQUEST_METHOD')
      method = request_headers['REQUEST_METHOD']
      request_headers.delete('REQUEST_METHOD')
    end
    path = ""
    if request_headers.include?('REQUEST_URI')
      path = request_headers['REQUEST_URI']
      request_headers.delete('REQUEST_URI')
    end

    # set up the Rack request
    env = Rack::MockRequest.env_for(path, {:method => method})
    output = String.new
    io = StringIO.new(output)
    if use_hijack
      # rack hijacking stuff
      env.merge!({ 'rack.hijack?' => true,
                   'rack.hijack' => Proc.new {},
                   'rack.hijack_io' => io })

    else
      # partial hijacking stuff
      env.merge!({ 'rack.hijack?' => true,
                   'rack.hijack' => lambda { raise NotImplementedError, "only partial hijack is supported."},
                   'rack.hijack_io' => nil })
    end

    # extra stuff that cgimap complains it needs
    env.merge!({ 'REMOTE_ADDR' => '127.0.0.1' })
    # any extra stuff from the test case
    env.merge!(request_headers)
    req = Rack::Request.new(env)

    # create the cgimap objects - normally these would be 
    # longer-lived than this, but for the purposes of testing
    # it's easier to re-create them each time.
    factory = Cgimap.create_backend({ 'backend' => 'staticxml',
                                      'file' => osm_data })
    limiter = Cgimap::RateLimiter.new({})
    routes = Cgimap::Routes.new
    actual_code, actual_headers, actual_body =
      Cgimap::process_request(req, limiter, "CgimapModuleTest", routes, factory)

    # get expected code and reason
    code, reason = response_headers['STATUS'].split(/ /, 2)

    if use_hijack
      # parse the response back
      # TODO: figure out some way of doing this without using
      # undocumented internals of Net::HTTP.
      reader = Net::InternetMessageIO.new(StringIO.new(output))
      response = Net::HTTPResponse.read_new(reader)
      response.reading_body(reader, method != "HEAD") {}

      # check the response reason where we have this
      assert_equal reason, response.message, "Reason / Status message"

      # actual headers object responds to hash lookups
      actual_code = response.code
      actual_headers = Hash[response.to_hash.map {|k, v| [k, v.join]}]
      actual_body = response.body

    else
      actual_body = actual_body.join
    end

    # check response code
    assert_equal code.to_i, actual_code.to_i, "Response code (expected=#{code.inspect}, actual=#{actual_code.inspect})"

    # normalise keys to upcase
    actual_headers = Hash[actual_headers.map {|k, v| [k.upcase, v]}]

    # check the headers in the test
    for k, v in response_headers
      # headers prefixed with ! denote negation
      if k.start_with?('!')
        v2 = actual_headers[k[1..-1]]
        assert_equal nil, v2

      elsif k != 'STATUS'
        v2 = actual_headers.fetch(k)
        assert_equal v, v2
      end
    end

    # and compare the body too
    content_type = response_headers['CONTENT-TYPE']
    unless content_type.nil?
      if content_type[0..7] == 'text/xml'
        compare_content_xml(body, actual_body)

      elsif content_type[0..8] == 'text/json'
        compare_content_json(body, actual_body)

      else
        flunk "Unknown content type in response: #{content_type.inspect}"
      end
    end
  end

  # this is some hacky reflection here to get this
  # class to pick up all the 'test/*.tescore/*.case'
  # files without needing a big list of them here.
  Dir.glob("test/*.testcore").each do |dir|
    if File.exist?(File.join(dir, "data.osm"))
      Dir.glob(File.join(dir, "*.case")) do |file|
        sym = (file.gsub(/[^a-z0-9]/, "_") + "_hijack").to_sym
        define_method(sym, Proc.new { run_testcore(file, true) })

        sym = (file.gsub(/[^a-z0-9]/, "_") + "_nohijack").to_sym
        define_method(sym, Proc.new { run_testcore(file, false) })
      end
    end
  end
end
