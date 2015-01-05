require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new "cgimap" do |ext|
  ext.lib_dir = "lib/cgimap"
end

Rake::TestTask.new do |t|
  t.libs << 'test'
end
