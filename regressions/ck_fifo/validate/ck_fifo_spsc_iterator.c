#include <ck_fifo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

struct example {
	int x;
};

static ck_fifo_spsc_t spsc_fifo;

int
main(void)
{
	int i, length = 3;
	struct example *examples;
	ck_fifo_spsc_entry_t *stub, *entries, *entry, *next;

	stub = malloc(sizeof(ck_fifo_spsc_entry_t));
	if (stub == NULL)
		exit(EXIT_FAILURE);
	
	ck_fifo_spsc_init(&spsc_fifo, stub);

	entries = malloc(sizeof(ck_fifo_spsc_entry_t) * length);
	if (entries == NULL)
		exit(EXIT_FAILURE);

	examples = malloc(sizeof(struct example) * length);
	/* Need these for this unit test. */
	if (examples == NULL)
		exit(EXIT_FAILURE);

	for (i = 0; i < length; ++i) {
		examples[i].x = i;
		ck_fifo_spsc_enqueue(&spsc_fifo, entries + i, examples + i);
	}

	puts("Testing CK_FIFO_SPSC_FOREACH.");
	CK_FIFO_SPSC_FOREACH(&spsc_fifo, entry) {
		printf("Next value in fifo: %d\n", ((struct example *)entry->value)->x);
	}

	puts("Testing CK_FIFO_SPSC_FOREACH_SAFE.");
	CK_FIFO_SPSC_FOREACH_SAFE(&spsc_fifo, entry, next) {
		if (entry->next != next)
			exit(EXIT_FAILURE);
		printf("Next value in fifo: %d\n", ((struct example *)entry->value)->x);
	}

	free(examples);
	free(entries);
	free(stub);

	return (0);
}

