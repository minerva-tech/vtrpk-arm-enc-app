/*
 * cmem_buf.h
 *
 *  Created on: Jan 26, 2013
 *      Author: a
 */

#ifndef CMEM_BUF_H_
#define CMEM_BUF_H_

struct CmemBuf { // copy-paste from CaptureBuf. todo: use single type
	typedef std::unique_ptr<CmemBuf> ptr;

	CmemBuf(size_t size_, CMEM_AllocParams& alloc_params_) :
		size(size_),
		user_addr(NULL),
		phy_addr(0),
		alloc_params(alloc_params_)
	{
		user_addr = CMEM_alloc(size, &alloc_params);

		if (user_addr) {
			phy_addr = CMEM_getPhys(user_addr);
			if (!phy_addr)
				throw ex("Failed to get phy cmem buffer address");
		} else {
			throw ex("Failed to allocate cmem buffer.");
		}
	}

	~CmemBuf() {
		if (user_addr)
			CMEM_free(user_addr, &alloc_params);
	}

	size_t				size;
	void*				user_addr;
	unsigned long 		phy_addr;
	CMEM_AllocParams 	alloc_params;
};

#endif /* CMEM_BUF_H_ */
