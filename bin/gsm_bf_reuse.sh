cids=`echo "select distinct(cellid) from keys;" | sqlite3 keys.db`
for cid in $cids; do
  keys=`echo "select key from keys where key not like '0000000000000000' and cellid=$cid;" | sqlite3 keys.db`
  empty=`echo "select file from keys where key like '0000000000000000' and cellid=$cid;" | sqlite3 keys.db`
  for file in $empty; do
    echo $file
    for key in $keys; do
      if gsm_convert -f new/$file -k $key -d 2>/dev/null |grep -q KEY_OK; then
        echo "update keys set key='$key' where file like '$file';"
      fi
    done
  done
done

