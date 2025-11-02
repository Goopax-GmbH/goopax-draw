#!/bin/sh


for i in particles.frag particles.vert particles_pot.vert overlay.frag overlay.vert; do
    glslang "$i" -V -o "$i.spv"
    xxdi.pl "$i.spv" | sed -re 's/unsigned/static unsigned/' > "$i.spv.cpp"
done

