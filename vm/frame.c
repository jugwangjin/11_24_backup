#include "vm/frame.h"
#include "threads/palloc.h"

void
frame_init (void)
{
  hash_init (&frame_table, frame_hash, frame_less, NULL);
  lock_init (&frame_lock);
}

unsigned
frame_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct frame_table_entry *f = hash_entry (p_, struct frame_table_entry, hash_elem);
  return hash_bytes (&f->kaddr, sizeof f->kaddr);
}

bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
            void *aux UNUSED)
{
  const struct frame_table_entry *a = hash_entry (a_, struct frame_table_entry, hash_elem);
  const struct frame_table_entry *b = hash_entry (b_, struct frame_table_entry, hash_elem);
  return a->kaddr < b->kaddr;
}

void
*frame_get_page (enum palloc_flags flags, struct spage_table_entry *spage)
{
  void *frame;
  struct frame_table_entry *entry;
  struct thread *thread_iter;
  uint32_t *pd;
//  struct hash_iterator clock_hand;
  
  frame = NULL;
  if (! (flags & PAL_USER))
    return false;
  frame = palloc_get_page (flags);
  if (frame != NULL)
  {
    frame_table_insert (frame, spage->uaddr);
    return frame;
  }
  else
  {
    if (clock_hand == NULL)
      hash_first (clock_hand, &frame_table);
    while (clock_hand != NULL)
    {
      entry = hash_entry (hash_cur (clock_hand), struct frame_table_entry, hash_elem);
      thread_iter = entry->thread;
      pd = thread_iter->pagedir;
      if (!entry->pin)
      {
        if (pagedir_is_accessed (pd, entry->uaddr))
          pagedir_set_accessed (pd, entry->uaddr, false);
        else
        {
          pagedir_set_accessed (pd, entry->uaddr, false);
          pagedir_clear_page (pd, entry->uaddr);
          frame = entry->kaddr;
          break;
        }
      }
    

    hash_next (clock_hand);
    if (clock_hand == NULL)
      hash_first (clock_hand, &frame_table);
    }
    return frame;
  }
  return NULL;
}

bool
frame_table_insert (void *frame, void *uaddr)
{
  struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));
  fte->kaddr = frame;
  fte->uaddr = uaddr;
  fte->thread = thread_current ();
  return hash_insert (&frame_table, &fte->hash_elem);
}

void
frame_free_page (void *frame)
{
  struct frame_table_entry fte;
  struct hash_elem *e;

  fte.kaddr = frame;
  e = hash_delete (&frame_table, &fte.hash_elem);
  if (e != NULL)
    free (hash_entry (e, struct frame_table_entry, hash_elem));
  palloc_free_page (frame);
}

