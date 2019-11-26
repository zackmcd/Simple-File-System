In our implementation, we decided to use numerous structs, a SuperBlock struct
which holds the high level information of each aspect of the file system, i.e.
where the Root Directory and data blocks are stored, the total number of
blocks, FAT blocks, and data blocks. We additionally created a FATBlock struct,
a FAT struct, a Root struct, and a FDTable structs. These all contain their
relevant information and gave us ease in accessing the important information
such as sizes and indexes of each separate block. 

In mount, we mount the disk by initializing the file descriptor table and super
block, and ensuring that the superblock has the correct signature and correct
amount of blocks. We additionally read in the fat entries, root directory, and
buffer into the fat and we check the completion of these requests. Umount is
the opposite, we write everything back to the disk, and return failure (-1) if
something is not properly saved.

For fs create and fs delete we used a helper function to check if a file was a
part of the root directory, and thus in the file system. If it wasn’t a part of
the filesystem, in create we would create the file with the appropriate
specifications (size = 0, the FAT is FAT EOC, and the filename). In delete we
looped through the indexes of the FAT looking for FAT EOC and then removed the
file from the root directory and the data blocks.

In fs write and read we follow a consistent structure that covers two cases:
when the offset + count is less than or equal to block size, meaning we only
need to read or write from one block, or offset + count is greater than block
size, in which case we are traversing numerous blocks. In write, if it is
greater then we traverse through the FAT and look for the index the offset is
at and perform the write. We have a variable leftOver, that we use to keep
track of whether or not we have more bits left to read or write. This leftOver
variable is used in a while loop that finds the next index to read or write to,
and performs the correct operation while also calculating the leftOver variable
and the total byte variable.We also move the buffer when writing or the block
when reading by moving the starting spot with pointer arithmetic.

To test, 
