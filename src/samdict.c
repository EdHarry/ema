#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "samrecord.h"
#include "main.h"
#include "util.h"
#include "samdict.h"

static SAMDictEnt *sde_new(SAMRecord *key, Cloud *v)
{
	SAMDictEnt *sde = safe_malloc(sizeof(*sde));
	sde->key = key;
	sde->mate = NULL;
	sde->cand_records[0] = key;
	sde->cand_clouds[0] = v;
	sde->gammas[0] = key->score;
	sde->num_cands = 1;
	sde->visited = 0;
	return sde;
}

SAMDict *sam_dict_new(void)
{
	SAMDict *sd = safe_malloc(sizeof(*sd));
	sam_dict_clear(sd);
	return sd;
}

static SAMDictEnt *find_for_key(SAMDict *sd, SAMRecord *k)
{
	const uint32_t idx = (record_hash(k) & (SAM_DICT_CAP - 1));
	for (SAMDictEnt *e = sd->entries[idx]; e != NULL; e = e->clash_next) {
		if (record_eq(k, e->key)) {
			return e;
		}
	}
	return NULL;
}

static SAMDictEnt *find_mate_for_key(SAMDict *sd, SAMRecord *k)
{
	const uint32_t idx = (record_hash_mate(k) & (SAM_DICT_CAP - 1));
	for (SAMDictEnt *e = sd->entries[idx]; e != NULL; e = e->clash_next) {
		if (record_eq_mate(k, e->key)) {
			return e;
		}
	}
	return NULL;
}

int sam_dict_add(SAMDict *sd, SAMRecord *k, Cloud *v, const int force)
{
	SAMDictEnt *e = find_for_key(sd, k);

	if (e != NULL) {
		const size_t num_cands = e->num_cands;
		if (num_cands < MAX_CANDIDATES) {
			if (num_cands > 0) {
				Cloud *parent = e->cand_clouds[num_cands - 1];

				if (parent == v && !force) {
					return 1;
				}

				/* create link in the disjoint-set structure */
				Cloud *root1 = parent;
				while (root1->parent != NULL) {
					root1 = root1->parent;
				}

				Cloud *root2 = v;
				while (root2->parent != NULL) {
					root2 = root2->parent;
				}

				if (root1 != root2) {
					Cloud *leaf = parent;
					while (leaf->child != NULL) {
						leaf = leaf->child;
					}

					root2->parent = leaf;
					leaf->child = root2;
				}
			}

			e->cand_clouds[num_cands] = v;
			e->cand_records[num_cands] = k;
			e->gammas[num_cands] = k->score;
			++(e->num_cands);
		}
	} else {
		const uint32_t idx = (record_hash(k) & (SAM_DICT_CAP - 1));
		e = sde_new(k, v);
		e->link_next = sd->head;
		sd->head = e;

		e->clash_next = sd->entries[idx];
		sd->entries[idx] = e;

		SAMDictEnt *mate = find_mate_for_key(sd, k);

		if (mate != NULL) {
			e->mate = mate;
			mate->mate = e;
		}

		++(sd->count);
	}

	return 0;
}

void sam_dict_del(SAMDict *sd, SAMRecord *k)
{
	SAMDictEnt *e = find_for_key(sd, k);

	if (e != NULL) {
		--(e->num_cands);
	}
}

void sam_dict_clear(SAMDict *sd)
{
	sd->head = NULL;
	memset(sd->entries, 0, SAM_DICT_CAP*sizeof(SAMDictEnt *));
	sd->count = 0;
}

SAMRecord *find_best_record(SAMDictEnt *e, double *gamma, struct xa *alts, size_t *n_alts)
{
	SAMRecord **cand_records = e->cand_records;
	double *gammas = e->gammas;
	size_t best = 0;
	double best_gamma = -1.0;
	struct xa *alt = &alts[0];
	*n_alts = 0;

	const size_t num_cands = e->num_cands;
	for (size_t i = 0; i < num_cands;) {
		double total_gamma = gammas[i];  // cloud splitting can lead to dup. records, so collect total prob.

		size_t j;
		for (j = i+1; j < num_cands; j++) {
			if (cand_records[j] == cand_records[i])
				total_gamma += gammas[j];
			else
				break;
		}

		if (total_gamma > best_gamma) {
			best = i;
			best_gamma = total_gamma;
		}

		i = j;
	}

	*gamma = best_gamma;

	if (best_gamma <= SECONDARY_ALIGN_THRESH) {
		size_t second_best = 0;
		double second_best_gamma = -1.0;
		for (size_t i = 0; i < num_cands;) {
			double total_gamma = gammas[i];  // cloud splitting can lead to dup. records, so collect total prob.

			size_t j;
			for (j = i+1; j < num_cands; j++) {
				if (cand_records[j] == cand_records[i])
					total_gamma += gammas[j];
				else
					break;
			}

			if (i != best && total_gamma > second_best_gamma) {
				second_best = i;
				second_best_gamma = total_gamma;
			}

			i = j;
		}

		if (second_best_gamma > 0) {
			SAMRecord *second_best_record = cand_records[second_best];
			alt->chrom = chrom_lookup(second_best_record->chrom);
			alt->pos = second_best_record->pos;
			alt->edit_dist = second_best_record->aln.edit_dist;
			alt->cigar_len = second_best_record->aln.n_cigar;
			assert((size_t)(alt->cigar_len) < sizeof(alt->cigar)/sizeof(alt->cigar[0]));
			alt->rev = second_best_record->rev;
			memcpy(alt->cigar, second_best_record->aln.cigar, alt->cigar_len * sizeof(alt->cigar[0]));
			++*n_alts;
		} else {
			alt->chrom = NULL;
			alt->pos = 0;
		}
	} else {
		alt->chrom = NULL;
		alt->pos = 0;
	}

	return cand_records[best];
}

