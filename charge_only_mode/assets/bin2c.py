import sys

if len(sys.argv) != 4:
	print 'Usage: %s name width height' % sys.argv[0]
	sys.exit(1)

NAME, WIDTH, HEIGHT = sys.argv[1:]

print 'static const unsigned char %s_bits[] = {' % NAME

while True:
	b = sys.stdin.read(16)
	if len(b) == 0: break
	print ''.join( [ '0x%02x,' % ord(c) for c in b ] )

print '};\n\nstruct asset %s = {\n\t%s, %s, %s_bits, sizeof(%s_bits) - 1, NULL };' % (NAME, WIDTH, HEIGHT, NAME, NAME)
