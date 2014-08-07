import sys

b = ''
while True:
	c = sys.stdin.read()
	if len(c) == 0: break
	b += c

if len(b) % 3 != 0:
	print 'incomplete data'
	sys.exit(1)

while len(b) > 0:
	red,green,blue = [ ord(_) for _ in b[:3] ]
	c = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
	sys.stdout.write(chr(c & 0xff))
	sys.stdout.write(chr((c >> 8) & 0xff))
	b = b[3:]
