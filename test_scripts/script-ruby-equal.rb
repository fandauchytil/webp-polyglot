f = File.new( $0, 'r+b' )

print( "Look at me now: " );
if f.pread( 1, IMG0 ) == "V"
  f.pwrite( 'U', IMG0 )
  f.pwrite( 'V', IMG1 )
  puts( "I'm Moon Moon! Let's HAAAAX!" );
else
  f.pwrite( 'V', IMG0 )
  f.pwrite( 'U', IMG1 )
  puts( "I Can Has 1337burger?" );
end

exit(0);

