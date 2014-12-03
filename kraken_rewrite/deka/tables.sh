echo "rft = np.array("

i=0
for line in `cat advances.txt | tr -s " " | cut -d " " -f 3,4 | sed -re "s/^([^ ]+) ([^ ]+)$/\2\1/"`; do
  if [ $(( $i % 8 )) -eq 0 ]; then
    echo "], ["
  fi
  echo "0x$line,"
  let i++
done

echo "]
        , dtype=np.uint64)"
