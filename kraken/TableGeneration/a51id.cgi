Your id is: 484192
--condition rounds:rounds=32 --roundfunc xor:condition=distinguished_point::bits=15:generator=lfsr2::tablesize=32::advance=484192 --implementation sharedmem --algorithm A51 --device cuda --operations 512 --work random:prefix=11,0 --consume file:prefix=data:append --logger normal generate --chains 270000000 --chainlength 3000000 --intermediate filter
