require 'socket'

s = UDPSocket.new
s.bind(nil, 1234)
while true
  text, sender = s.recvfrom(16)
  puts text
end
