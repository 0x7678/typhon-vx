# pipe in output of bts_scan_jenda.sh
# then pass resulting files to qlandkartegt (or your favorite mapping software)
# example: http://jenda.hrach.eu/brm/bts-gsm.jpg

locdir="locs/"
[ -d $locdir ] || mkdir $locdir

magic_pre='<?xml version="1.0" encoding="UTF-8"?>
<loc version="1.0" src="Deka Consulting">
<waypoint>
        <name id="'
magic_inter='">
</name>
<coord lon="'
magic_post='"/>
<type>BTS</type>
<link text="Details"></link>
</waypoint></loc>'

files=""

while read line; do
  mcnc=`echo "$line" | cut -d ";" -f 2`
  cid=`echo "$line" | cut -d ";" -f 4`
  case "$mcnc" in
    23001)
      opname="t-mobile"
      ;;
    23002)
      opname="eurotel"
      ;;
    23003)
      opname="oskar"
      ;;
    *)
      echo "WTF is $mcnc ?"
      exit 1
      ;;
  esac
  fname="$locdir$mcnc-$cid.loc"
  if [ -f "$fname" ]; then
    echo "DBG: $fname already exists"
  else
    page=`wget --post-data="udaj=${cid}&par=cid&op=${opname}&gps=&foto=&razeni=original&smer=vzestupne" http://www.gsmweb.cz/search.php -O - -q`
    magic_magic=`echo "$page" | grep -oE 'HREF="http://www.mapy.cz/#d=coor_[0-9]{1,3}\.[0-9]{3,15},[0-9]{1,3}\.[0-9]{3,15}_1&amp;l=16' | grep -oE 'coor_[0-9]{1,3}\.[0-9]{3,15},[0-9]{1,3}\.[0-9]{3,15}' | cut -c 6- | sed -re 's/,/" lat="/'`
    echo "$magic_pre$line$magic_inter$magic_magic$magic_post" > "$fname"
  fi
  files="$files $fname"
done

echo "Processed BTS's:"
echo "$files"

