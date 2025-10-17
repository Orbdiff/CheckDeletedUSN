# CheckDeletedUSN

Designed to parse the **`$UsnJrnl:$J`** file in the NTFS system and determine whether the *USN Journal* has been deleted.

The tool accesses **`C:\$Extend\$UsnJrnl:$J`**, reads its **creation timestamp**, and compares it against the **system boot time**, if the `$UsnJrnl` creation time is **later than the boot time**, it is very likely that the USN Journal has been deleted.