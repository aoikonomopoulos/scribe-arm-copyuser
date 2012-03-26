#!/usr/bin/env ruby

if ARGV.size != 1
  $stderr.puts("usage: #{$0} name_of_executable")
  exit(2)
end

prog = ARGV.shift

testcases = IO.popen("#{prog} list", "r").readlines.collect { |l|
  l.chomp
}

testcases.each { |tc|
  system("#{prog} #{tc}")
  if $? != 0
    $stderr.puts("Testcase #{tc} did not execute successfully (#{$?})")
    exit(1)
  end
}
exit(0)

