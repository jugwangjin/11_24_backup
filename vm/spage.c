#include <hash.h>
#include "vm/spage.h"
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "threads/thread.h"
#include "threads/palloc.h"

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void
init_spage (struct hash *spage_table)
{
  hash_init (spage_table, spage_hash, spage_less, NULL);
}

unsigned
spage_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spage_table_entry *s = hash_entry (p_, struct spage_table_entry, hash_elem);
  return hash_int ((int)&s->uaddr);
}

bool
spage_less (const struct hash_elem *a_, const struct hash_elem *b_,
            void *aux UNUSED)
{
  const struct spage_table_entry *a = hash_entry (a_, struct spage_table_entry, hash_elem);
  const struct spage_table_entry *b = hash_entry (b_, struct spage_table_entry, hash_elem);
  return a->uaddr < b->uaddr;
}

struct spage_table_entry *
get_spage (struct hash *spage_table, void *uaddr)
{
  struct spage_table_entry s;
  struct hash_elem *e;
  
  s.uaddr = pg_round_down (uaddr);
  e = hash_find (spage_table, &s.hash_elem);
  return e != NULL ? hash_entry (e, struct spage_table_entry, hash_elem) : NULL;
}

bool
make_spage_for_stack_growth (struct hash *spage_table, void *fault_addr)
{
  struct spage_table_entry *ste = malloc (sizeof (struct spage_table_entry));
  if (!ste)
    return false;
  ste->uaddr = pg_round_down (fault_addr);
  ste->writable = true;
  ste->mmap = false;
  ste->file = false;
  ste->swap = false;
  
  hash_insert (spage_table, &ste->hash_elem);
  
  void *allocated_frame = frame_get_page (PAL_USER, ste->uaddr);
  if (!allocated_frame)
  {
    spage_free_page (ste->uaddr, spage_table);
    return false;
  }
  if (!install_page (ste->uaddr, allocated_frame, ste->writable))
  {
    spage_free_page (ste->uaddr, spage_table);
    return false;
  }
  return true;
}

void
spage_free_page (void *uaddr, struct hash *spage_table)
{
  struct spage_table_entry ste;
  struct hash_elem *e;
  
  ste.uaddr = uaddr;
  e = hash_delete (spage_table, &ste.hash_elem);
  if (e != NULL)
    free (hash_entry (e, struct spage_table_entry, hash_elem));
}

bool
load_file (struct spage_table_entry *ste, void *frame)
{
  if (file_read_at (ste->file_ptr, frame, ste->read_bytes, ste->ofs) != (int) ste->read_bytes)
  {
    frame_free_page (frame);
    return false;
  }
  memset (frame + ste->read_bytes, 0, ste->zero_bytes);
  return true;
}

bool
spage_get_frame (struct spage_table_entry *ste)
{
  void *allocated_frame;
  bool success;
  
  allocated_frame = frame_get_page (PAL_USER, ste->uaddr);
  
  if (!allocated_frame)
    return false;
  success = false; 
  if (ste->file)
    success = load_file (ste, allocated_frame);
    

  success = install_page (ste->uaddr, allocated_frame, ste->writable);  
  if (success == false)
    frame_free_page (allocated_frame);
  return success;
}
     
