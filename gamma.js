// Based on formula from 
// https://learn.adafruit.com/led-tricks-gamma-correction/the-longer-fix
 
var   gamma   = 2.8;  // Correction factor
var   max_in  = 1023, // Top end of INPUT range
      max_out = 1023;  // Top end of OUTPUT range
 
//process.stdout.write("static const uint16_t s_gammaTable[] = {");
for (var i=0; i<=max_in; i++) {
  if (i > 0) 
    process.stdout.write(', ');
  if ((i & 15) == 0)
    process.stdout.write("\r\n  ");
  var tmp = Math.floor(Math.pow(i / max_in, gamma) * max_out + 0.5).toString(10);
  process.stdout.write("   ".substr(tmp.length-1) + tmp);
}
//process.stdout.write("\r\n};\r\n");
