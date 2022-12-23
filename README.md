# MyLZARI
This is an amateur implementation of LZARI, based on online readings and University lectures.
The program is meant for an exercise in the university's Information Theory course.
It combines LZSS with Arithmetic Coding, just like Haruhiko Okumara's original implementation (only, probably, at worse performance).
I'd like to thank Prof. Or Ordentlich for teaching me and the rest of the students in this course multiple techniques for compression, universal like this one and non-univesal, while relating them to theoretical measures and providing preformance bounds.

## LZSS
Based on LZ77, the slight algorithmic change is to encode literal symbols literally when there is no match in the previously seen bytes,
and to put some lower bound on the length of the found match (a break even point).
These are both implemented here, but in the simplest forms. RAR, for example, has some growth factor to the break even point, depending on the found distance - if the distance is greated than 255, break even is 2 bytes; if greater than 4095, break even is 3; 255K, break even is 4; etc. This way we're less likely to have large distances (which are going to have lower empirical probability) - we'll have them only if they're really worth it, i.e save us from writing literals.
I kept the break-even at 2. This is always better than the original since if the length is 1, we'd rather just encode the original number, and if the length is 2, it doesn't matter if we encode 2 bytes or <distance,length> (actually from a probabilistic viewpoint it's better).

I took Okumara's method of dividing the symbol space (which is set by a macro) so that the first 256 symbols represent literals and the rest represent distances and lengths. To get the actual values we subtract 256 from the symbol. Luckily, with using LZSS we get 256 as the EOF symbol (we'll never get a zero (256-256) distance or length).

## Arithmetic coding
This is the more expansive and difficult task. Encoding the LZ output straightforawrdly will be better than just using bytes, probably. I'm saying "probably" because we are expanding the symbol set size when using distances and lengths as more symbols over the actual literals.
Say our symbol space is 10 bits, 00xxxxxxxx for literals and the rest for dist/len. We can just write 10 bit values, one by one. A better way is to use arithmetic coding.
Using arithmetic coding in practice is very difficult, because of precision issues. There are ways written on the internet to overcome this, and you can look into the code to see how I did it. The basic idea is this:
We want to mimic an interval betwen 0 and 1. So we want to mimic the interval 0.00000000... to 1.00000000...
We set the precision we'll go by and "right-shift" the interval by it. Say the percision is 8, so we'll mimic the interval 000000000 to 100000000
Now we can encode an 8-bit estimation of our interval!
Rescaling is needed per symbol - through the encoding process, rescale back to 8 bit percision, while emitting the bits we would disregard in the process.
There are multiple subtleties to this process, but that's the main idea.
