#!/bin/sh


for i in particles.frag particles.vert particles_pot.vert overlay.frag overlay.vert; do
    glslang "$i" -V -o "$i.spv"
    xxdi.pl "$i.spv" > "$i.spv.cpp"
done

