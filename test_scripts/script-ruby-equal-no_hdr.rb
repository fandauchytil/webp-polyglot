f = File.new($0, 'r+b')

f.seek(1, :CUR)

f.write('f')
print ("Hack the planet!");

exit(0);
