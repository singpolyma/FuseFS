require 'mkmf'
$defs << "-DRUBY_VERSION_CODE=#{RUBY_VERSION.gsub(/\D/, '')}"
dir_config('fusefs_lib.so')
if have_library('fuse') 
  create_makefile('fusefs_lib')
else
  puts "No FUSE install available"
end
