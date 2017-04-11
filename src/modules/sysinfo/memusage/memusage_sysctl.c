#include "memusage.h"

#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <vm/vm_param.h>
#endif

#if defined(__OpenBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
# include <sys/swap.h>
# include <sys/mount.h>
#endif 

#if defined(__FreeBSD__) || defined(__DragonFly__)
static long int
_sysctlfromname(const char *name, void *mib, int depth, size_t *len)
{
    long int result; 
   
    if (sysctlnametomib(name, mib, len) < 0) return -1;

    *len = sizeof(result);
    if (sysctl(mib, depth, &result, len, NULL, 0) < 0) return -1;

    return result;
}
#endif

#if defined(__OpenBSD__)
void _memsize_bytes_to_kb(unsigned long *bytes)
{
     *bytes = (unsigned int) *bytes >> 10;
}
#endif

void _memusage_sysctl_getusage(unsigned long *mem_total,
                             unsigned long *mem_used,
                             unsigned long *mem_cached,
                             unsigned long *mem_buffers,
                             unsigned long *mem_shared,
                             unsigned long *swp_total,
                             unsigned long *swp_used)
{
   size_t len;
   int i = 0;
   *mem_total = *mem_used = *mem_cached = *mem_buffers = *mem_shared = 0;
   *swp_total = *swp_used = 0;
#if defined(__FreeBSD__) || defined(__DragonFly__)
   int total_pages = 0, free_pages = 0, inactive_pages = 0;
   long int result = 0;
   int page_size = getpagesize();

   int *mib = malloc(sizeof(int) * 4);
   if (mib == NULL) return;

   mib[0] = CTL_HW; mib[1] = HW_PHYSMEM; 
   len = sizeof(*mem_total);
   if (sysctl(mib, 2, mem_total, &len, NULL, 0) == -1)
     return;
   *mem_total /= 1024;

   total_pages = _sysctlfromname("vm.stats.vm.v_page_count", mib, 4, &len);
   if (total_pages < 0) return;

   free_pages = _sysctlfromname("vm.stats.vm.v_free_count", mib, 4, &len);
   if (free_pages < 0) return;

   inactive_pages = _sysctlfromname("vm.stats.vm.v_inactive_count", mib, 4,  &len);
   if (inactive_pages < 0) return;

   *mem_used = (total_pages - free_pages - inactive_pages) * page_size >> 10; 
  
   result = _sysctlfromname("vfs.bufspace", mib, 2, &len);
   if (result < 0) return;
   *mem_buffers = (result / 1024);

   result = _sysctlfromname("vm.stats.vm.v_active_count", mib, 4, &len); 
   if (result < 0) return;
   *mem_cached = ((result * page_size) / 1024);

   result = _sysctlfromname("vm.stats.vm.v_cache_count", mib, 4, &len);
   if (result < 0) return;
   *mem_shared = (result * page_size) / 1024;

   result = _sysctlfromname("vm.swap_total", mib, 2, &len);
   if (result < 0) return;
   *swp_total = (result / 1024);

   struct xswdev xsw;
   // previous mib is important for this one...

   while (i++) 
     {
        mib[2] = i;
        len = sizeof(xsw);
        if (sysctl(mib, 3, &xsw, &len, NULL, 0) == -1) break;

        *swp_used += xsw.xsw_used * page_size;
     }

   *swp_used >>= 10;

   free(mib);
#elif defined(__OpenBSD__)
   static int mib[] = { CTL_HW, HW_PHYSMEM64 };
   static int bcstats_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   static int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   int nswap, rnswap;
   struct swapent *swdev = NULL;

   len = sizeof(*mem_total);
   if (sysctl(mib, 2, mem_total, &len, NULL, 0) == -1)
     return;
   
   len = sizeof(uvmexp);
   if (sysctl(uvmexp_mib, 2, &uvmexp, &len, NULL, 0) == -1)
     return;
    
   len = sizeof(bcstats);
   if (sysctl(bcstats_mib, 3, &bcstats, &len, NULL, 0) == -1)
     return;

   // Don't fail if there's not swap!
   nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap == 0) goto swap_out;
   
   swdev = calloc(nswap, sizeof(*swdev));
   if (swdev == NULL) goto swap_out;

   rnswap = swapctl(SWAP_STATS, swdev, nswap);
   if (rnswap == -1) goto swap_out;

   for (i = 0; i < nswap; i++)// nswap; i++)
      {
         if (swdev[i].se_flags & SWF_ENABLE)
           {
              *swp_used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
              *swp_total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
           }
      }
swap_out:
   if (swdev) free(swdev);

   *mem_total /= 1024;
 
   *mem_cached  = (uvmexp.pagesize * bcstats.numbufpages);
   _memsize_bytes_to_kb(mem_cached);

   *mem_used  = (uvmexp.active * uvmexp.pagesize);
   _memsize_bytes_to_kb(mem_used);

   *mem_buffers = (uvmexp.pagesize * (uvmexp.npages - uvmexp.free));
   _memsize_bytes_to_kb(mem_buffers);

   *mem_shared = (uvmexp.pagesize * uvmexp.wired);
   _memsize_bytes_to_kb(mem_shared);
#endif 
}
