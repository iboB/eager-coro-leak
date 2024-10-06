require 'net/http'
require 'json'

src = File.read('main.cpp')

if !ARGV[0]
  puts 'Usage: ruby run-on-ce.rb <compiler-id>'
  puts
  puts 'Useful compiler ids:'
  puts
  puts 'g114      - x86-64 gcc 11.4'
  puts 'g133      - x86-64 gcc 13.3'
  puts 'g142      - x86-64 gcc 14.2'
  puts 'gsnapshot - x86-64 gcc (trunk)'
  puts 'clang1500 - x86-64 clang 15.0.0'
  puts 'clang1600 - x86-64 clang 16.0.0'
  puts 'clang1701 - x86-64 clang 17.0.1'
  puts 'clang1810 - x86-64 clang 18.1.0'
  puts 'clang1910 - x86-64 clang 19.1.0'
  puts 'vcpp_v19_latest_x64 - x64 msvc latest'
  exit
end

compiler = ARGV[0]
std = compiler['vcpp'] ? '-std:c++20' : '-std=c++20'

data = {
  source: src,
  options: {
    userArguments: std,
    filters: {
      execute: true,
    },
    compilerOptions: {
      executorRequest: true,
    },
  },
}

url = URI.parse "https://godbolt.org/api/compiler/#{compiler}/compile"
result = Net::HTTP.post url, data.to_json, 'Content-Type' => 'application/json'
puts result.body
