/*--------------------------------------------------------------*/
/*--------------------------- ux_file.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include "ux_fs.h"

const struct file_operations ux_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
};

int ux_get_block(struct inode *inode, sector_t block,
		struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ux_inode *uip = (struct ux_inode *)inode->i_private;
	__u32 blk;

	pr_debug("uxfs: start %s with inode=%lu block=%lu create=%d\n",
		__func__, inode->i_ino, (long unsigned int)block, create);

	/*
	 * First check to see if the file can be extended.
	 */

	if (block >= UX_DIRECT_BLOCKS)
		return -EFBIG;

	/*
	 * If we're creating, we must allocate a new block.
	 */

	if (create) {
		blk = ux_data_alloc(sb);
		if (blk == 0) {
			pr_err("uxfs: %s - Out of space\n", __func__);
			return -ENOSPC;
		}
		uip->i_addr[block] = blk;
		uip->i_blocks++;
		inode->i_blocks++;
		uip->i_size = inode->i_size;
		mark_inode_dirty(inode);
	}

	map_bh(bh_result, sb, uip->i_addr[block]);
	if (create)
		set_buffer_new(bh_result);

	return 0;
}

int ux_writepage(struct page *page, struct writeback_control *wbc)
{
	pr_debug("uxfs: start %s\n", __func__);

	return block_write_full_page(page, ux_get_block, wbc);
}

int ux_readpage(struct file *file, struct page *page)
{
	pr_debug("uxfs: start %s with inode=%lu\n",
		__func__, file->f_inode->i_ino);

	return block_read_full_page(page, ux_get_block);
}

int ux_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned int len, unsigned int flags,
			struct page **pagep, void **fsdata)
{
	pr_debug("uxfs: start %s with inode=%lu pos=%lld len=%d\n",
		__func__, file->f_inode->i_ino, pos, len);

	return block_write_begin(mapping, pos, len, flags, pagep, ux_get_block);
}

sector_t ux_bmap(struct address_space *mapping, sector_t block)
{
	pr_debug("uxfs: start %s with inode=%lu block=%lu\n",
		__func__, mapping->host->i_ino, (long unsigned int)block);

	return generic_block_bmap(mapping, block, ux_get_block);
}

const struct address_space_operations ux_aops = {
	.readpage	= ux_readpage,
	.writepage	= ux_writepage,
	.write_begin	= ux_write_begin,
	.write_end	= generic_write_end,
	.bmap		= ux_bmap,
};

const struct inode_operations ux_file_inops = {
	.link	= ux_link,
	.unlink	= ux_unlink,
};
