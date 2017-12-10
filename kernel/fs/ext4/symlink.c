/*
 *  linux/fs/ext4/symlink.c
 *
 * Only fast symlinks left here - the rest is done by generic code. AV, 1999
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/symlink.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext4 symlink handling code
 */

#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/namei.h>
#include "ext4.h"
#include "xattr.h"

#ifdef CONFIG_EXT4_FS_ENCRYPTION
static void *ext4_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct page *cpage = NULL;
	char *caddr, *paddr = NULL;
	struct ext4_str cstr, pstr;
	struct inode *inode = dentry->d_inode;
	struct ext4_encrypted_symlink_data *sd;
	loff_t size = min_t(loff_t, i_size_read(inode), PAGE_SIZE - 1);
	int res;
	u32 plen, max_size = inode->i_sb->s_blocksize;

	if (!ext4_encrypted_inode(inode))
		return page_follow_link_light(dentry, nd);

	res = ext4_get_encryption_info(inode);
	if (res)
		return ERR_PTR(res);

	if (ext4_inode_is_fast_symlink(inode)) {
		caddr = (char *) EXT4_I(dentry->d_inode)->i_data;
		max_size = sizeof(EXT4_I(dentry->d_inode)->i_data);
	} else {
		cpage = read_mapping_page(inode->i_mapping, 0, NULL);
		if (IS_ERR(cpage))
			return cpage;
		caddr = kmap(cpage);
		caddr[size] = 0;
	}

	/* Symlink is encrypted */
	sd = (struct ext4_encrypted_symlink_data *)caddr;
	cstr.name = sd->encrypted_path;
	cstr.len  = le32_to_cpu(sd->len);
	if ((cstr.len +
	     sizeof(struct ext4_encrypted_symlink_data) - 1) >
	    max_size) {
		/* Symlink data on the disk is corrupted */
		res = -EIO;
		goto errout;
	}
	plen = (cstr.len < EXT4_FNAME_CRYPTO_DIGEST_SIZE*2) ?
		EXT4_FNAME_CRYPTO_DIGEST_SIZE*2 : cstr.len;
	paddr = kmalloc(plen + 1, GFP_NOFS);
	if (!paddr) {
		res = -ENOMEM;
		goto errout;
	}
	pstr.name = paddr;
	pstr.len = plen;
	res = _ext4_fname_disk_to_usr(inode, NULL, &cstr, &pstr);
	if (res < 0)
		goto errout;
	/* Null-terminate the name */
	if (res <= plen)
		paddr[res] = '\0';
	nd_set_link(nd, paddr);
	if (cpage) {
		kunmap(cpage);
		page_cache_release(cpage);
	}
	return NULL;
errout:
	if (cpage) {
		kunmap(cpage);
		page_cache_release(cpage);
	}
	kfree(paddr);
	return ERR_PTR(res);
}

static void ext4_put_link(struct dentry *dentry, struct nameidata *nd,
			  void *cookie)
{
	struct page *page = cookie;

	if (!page) {
		kfree(nd_get_link(nd));
	} else {
		kunmap(page);
		page_cache_release(page);
	}
}
#endif

static void *ext4_follow_fast_link(struct dentry *dentry, struct nameidata *nd)
{
	struct ext4_inode_info *ei = EXT4_I(dentry->d_inode);
	nd_set_link(nd, (char *) ei->i_data);
	return NULL;
}

const struct inode_operations ext4_symlink_inode_operations = {
	.readlink	= generic_readlink,
#ifdef CONFIG_EXT4_FS_ENCRYPTION
	.follow_link    = ext4_follow_link,
	.put_link       = ext4_put_link,
#else
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
#endif
	.setattr	= ext4_setattr,
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ext4_listxattr,
	.removexattr	= generic_removexattr,
	.set_gps_location	= ext4_set_gps,
	.get_gps_location	= ext4_get_gps,
};

const struct inode_operations ext4_fast_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link    = ext4_follow_fast_link,
	.setattr	= ext4_setattr,
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ext4_listxattr,
	.removexattr	= generic_removexattr,
	.set_gps_location	= ext4_set_gps,
	.get_gps_location	= ext4_get_gps,
};
