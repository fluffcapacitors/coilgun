# coilgun
This project was originally started in 2014. The initial code was last modified in 2016 to present at san diego maker faire and hasn't been touched since.

#components
A lot of the parts used are either EOL or the shopping links have long since changed. None of the parts are parcitularly specialized though, so here's a list of the major components and some of the reasoning behind the choices:

Batteries:
Since this design is low voltage/high current, batteries with a very low internal resistance and very high current rating are the heart of the build. These came in a convenient 2-pack on amazon (note there's now an 8000mah 120C version): https://www.amazon.com/gp/product/B09N938GDC/

Projectile:
Because this design is such low voltage (therefore the coils take longer to reach maximum field strength than higher voltage designs), we want to store the kinetic energy into the mass of the projectile rather than the velocity. These 12mm dowel pins from mcmaster have worked pretty well, and they're convenient to fitting inside 1/2 inch inner diameter tubes for the barrel: https://www.mcmaster.com/products/dowels/dowel-pins-2~/diameter~12-0000-mm/diameter~12-mm/length~50-mm/

Barrel:
The thinner the barrel wall the better, as it will allow the coils to be as close to the projectile as possible. We used a 1/2inch ID carbon fiber tube and noticed a substantial improvement over a thicker walled teflon tube. If I was building the gun again I'd try to make the tube even thinner (it's not like it's containing an explosion or anything. As long as the coils are rigidly mounted it doesn't need much structure to it)

Coils:
Through a lot of empirical testing at this particular voltage, we got the best results using a coil about the length of the projectile. 3 layers of 14 gauge wire worked best for the first two coils, and we noticed a slight improvement dropping it down to 2 layers for the 3rd coil. We found that winding the coils consistently and carefully actually got us another foot or two per second on average. 

Mosfets:
I went with 75V and the highest current I could find at the time (these were sourced in 2014). The model I think we eventually settled on was the IXFN520N075T2. Lower specced models tended to burn up, though I suspect that was more because we weren't using a gate driver back then. One of the things on the todo list is testing of smaller/cheaper mosfets but with a proper gate driver this time.

Gate driver:
I think this is the equivalent of what we got (I can't find them in my order history): https://www.mouser.com/ProductDetail/onsemi-Fairchild/FOD8318?qs=A6ThuIxGArAttosN%252BonLfQ%3D%3D Going by memory though, there weren't many good options on mouser (at least not back in 2014) that weren't surface mount. Here's some bonus video of a mosfet catching on fire before we knew to use gate drivers: https://www.youtube.com/watch?v=dqWuA1y4Sjs

Photodiodes:
If I was building the gun again, I'd look for one that can run off 3.3v so I could keep everything at 3.3v logic, but the one we're using now is the OPL810: https://www.mouser.com/ProductDetail/Optek-TT-Electronics/OPL810?qs=NVJATC80C4%252B1mVwTlwdVTg%3D%3D

IR LEDs:
Just any random 935mm IR LED to match the photodiode. These are what we used and they've been just fine: https://www.mouser.com/ProductDetail/Optek-TT-Electronics/OP165C?qs=N5kmjX%2FbzE76PE3bI0KNwA%3D%3D

Flyback diodes:
There may be room for improvement here. I don't really know what I'm doing, so we ended up putting one flyback diode in parallel with each coil plus 2 in series across the main busbars (in series because if one failed closed we'd short the batteries). I just bought the beefiest ones I could find in 2014, I believe it's this model: https://www.mouser.com/ProductDetail/Vishay-General-Semiconductor/P600M-E3-54?qs=E3OxquYwg8Gk%2Ff5OP6Wq7A%3D%3D

Misc:
The microcontroller was a teensy 3.2 because it's 5v tolerant (if I built this again I may try to make everything 3.3v). There are a lot of safety considerations I made in the code, such as going into an error shutdown state if any coil is on for longer than 10ms, or the shot takes longer than 20ms, or more than a single IR gate is triggered at once. Those red keys we were turning were car battery isolators: https://www.amazon.com/gp/product/B00U5UBREI . Then everything disconnectable uses an XT90 connector: https://www.amazon.com/gp/product/B08L3RS5HP
