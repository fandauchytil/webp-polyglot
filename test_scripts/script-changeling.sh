if [ "$(dd if="$0" ibs=1 skip="$IMG1" status=noxfer count=1 2>/dev/null)" = "V" ]; then
    l1="U"
    l2="V"
else
    l1="V"
    l2="U"
fi

echo -n "$l1" | dd of="$0" obs=1 seek="$IMG1" status=noxfer count=1 conv=notrunc 2>/dev/null
echo -n "$l2" | dd of="$0" obs=1 seek="$IMG2" status=noxfer count=1 conv=notrunc 2>/dev/null

exit 0  

