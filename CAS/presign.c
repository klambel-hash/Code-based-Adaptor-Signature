#include "presign.h"

void phiUV_sign(uint8_t *e, uint8_t *eU, uint8_t *eV, wave_sk_t *sk) {
	int i;
	int length = sk->HU->n_cols;
	for (i = 0; i < length; i++) {
		e[i] = f3_add(f3_mul(sk->coeff.a[i], eU[i]),
				f3_mul(sk->coeff.b[i], eV[i]));
		e[i + length] = f3_add(f3_mul(sk->coeff.c[i], eU[i]),
				f3_mul(sk->coeff.d[i], eV[i]));
	}
}

// v a vector of size 2 * length
// number of non-zero pairs (v[i], v[i+length])
int nz_pairs(uint8_t *v, int length) {
	int i, l;
	for (i = 0, l = 0; i < length; ++i)
		if ((v[i] == 0) && (v[i + length] == 0))
			++l;
	return length - l;
}

// v is a vector of size length.
// support will contain all integers from 0 to length-1.
// Non-zero postions of v come first in support.
// Side effect: weight is set to the Hamming weight of v.
void supp(uint8_t *v, int length, int *weight, unsigned int *support) {
	int i, j, l;
	//int *support = malloc(length * sizeof(int));
	for (j = 0, i = 0, l = length - 1; j < length; ++j) {
		if (v[j] != 0) {
			support[i] = j;
			++i;
		} else {
			support[l] = j;
			--l;
		}
	}
	if (weight != NULL)
		*weight = i;
	//return support;
}

void decodeV(uint8_t *eV, uint8_t *sV, mf3 *HV, prng_t *PRNG) {
	int i, t;
	uint8_t y[N - K] = { 0 };
	unsigned int support[N2] = { 0 };
	for (int i = 0; i < N2; ++i)
		support[i] = i;
	mf3 *H = mf3_augment(HV, sV);
	//save_matrix(H);
	//uint8_t eV[N2+1];
	//uint8_t *eV = (uint8_t*) malloc((N2 + 1) * sizeof(uint8_t));
	eV[N2] = f3_neg(1); // -1
	do {
		rand_shuffle(support, N2, N2 - KV + D, PRNG);
	} while (mf3_gauss_elim(H, support) > N2 - KV + D);
	//save_matrix(H);
	/*
	 mf3_gauss_elim() returns the number of pivots that were tried
	 before the Guassian elimination ended.

	 If d is large enough (the fully proven variant of Wave) the
	 above condition succeeds with probability overwhelmingly close to 1.

	 If d = 0 (the 'no gap' variant) we repeat the Gaussian
	 elimination until no pivot fails.
	 */
	while (1) {
		t = pickV(PRNG);
		for (i = 0; i < N2 - KV; ++i)
			eV[support[i]] = 0;
		for (; i < N2 - KV + D; ++i)
			eV[support[i]] = f3_rand(PRNG);
		for (; i < N2 - t; ++i)
			eV[support[i]] = 0;
		for (; i < N2; ++i)
			eV[support[i]] = f3_randnonzero(PRNG);
		mf3_ma_mul(H, eV, y);
		for (i = 0; i < N2 - KV; ++i)
			eV[support[i]] = f3_neg(y[i]);
		//free(y);
		if (acceptV(f3_array_weight(eV, N2), PRNG))
			break;
		rejV++; //updating the rejection
	}
	//free(support);
	mf3_free(H);
	//return eV;
}

void decodeU(uint8_t *e, uint8_t *sU, uint8_t *eV, mf3 *HU, wave_sk_t *sk,
		prng_t *PRNG) {
	int i, t, k_nz;
	uint8_t y[N - K];
	unsigned int suppV[N2] = { 0 };
	supp(eV, N2, &t, suppV);
	unsigned int support[N2] = { 0 }; //(int *) malloc(N2 * sizeof(int));
	uint8_t eU[N2 + 1] = { 0 }; // (int8_t *) malloc((N2 + 1) * sizeof(int8_t));
	/*
	 supp(eV, n2, &t) returns an array of all coordinates
	 {0, 1 ... n2-1} starting with the non zero positions of eV.
	 Side effect: the value of t is set to the weight of eV.
	 */

	mf3 *H = mf3_augment(HU, sU);

	eU[N2] = f3_neg(1); // -1
	while (1) {
		k_nz = pickU(t, PRNG);
		do {
			rand_shuffle(suppV, t, t - k_nz, PRNG);
			rand_shuffle(suppV + t, N2 - t, N2 - KU + D - (t - k_nz), PRNG);
			memcpy(support, suppV, (t - k_nz) * sizeof(unsigned int));
			memcpy(support + (t - k_nz), suppV + t,
					(N2 - t) * sizeof(unsigned int));
			memcpy(support + (N2 - k_nz), suppV + (t - k_nz),
					k_nz * sizeof(unsigned int));
			/*
			 Before the Gaussian elimination, support[] has
			 1- (t - k_nz) elements of support(e_V) chosen uniformly at
			 random in its first n2 - kU + d entries
			 2- (n2 - kU + d - (t - k_nz)) elements out of support(e_V)
			 chosen uniformly at random in its first n2 - kU + d
			 entries
			 */
		} while (mf3_gauss_elim(H, support) > N2 - KU + D);
		/*
		 After the Gaussian elimination, support[] still verifies the
		 above two conditions and in addition
		 3- its first (n2 - kU) entries are the pivots positions

		 If d is large enough (the fully proven variant of Wave) the
		 elimination succeed with probability overwhelmingly close to 1.

		 If d = 0 (the 'no gap' variant) we repeat the Gaussian
		 elimination until no pivot fails.
		 */
		do {
			for (i = 0; i < N2 - KU; ++i)
				eU[support[i]] = 0;
			for (; i < N2 - KU + D; ++i)
				eU[support[i]] = f3_rand(PRNG);
			for (; i < N2; ++i)
				if (eV[support[i]] == 0)
					eU[support[i]] = f3_randnonzero(PRNG);
				else
					eU[support[i]] = f3_mul(sk->coeff.x[support[i]],
							eV[support[i]]);
			mf3_ma_mul(H, eU, y);
			for (i = 0; i < N2 - KU; ++i)
				eU[support[i]] = f3_neg(y[i]);
			//free(y);
			phiUV_sign(e, eU, eV, sk);
		} while (f3_array_weight(e, N) != W);
		if (acceptU(t, nz_pairs(e, N2), PRNG))
			break;
		rejU++;
	}

	mf3_free(H);
}


//TODO fix eprime
void sign_wave(f3_vector *e, f3_vector *s, wave_sk_t *sk, prng_t *PRNG) {
	uint8_t eV[N2 + 1] = { 0 };
	uint8_t eprime[N2 * 2] = { 0 };
	uint8_t sprime[N - K] = { 0 };
	mf3_mv_mul(sk->Sinv, s, sprime);
	decodeV(eV, sprime + (N2 - KU), sk->HV, PRNG);
	decodeU(eprime, sprime, eV, sk->HU, sk, PRNG);

	for (int i = 0; i < N; ++i) {
		f3_vector_setcoeff(e, i, eprime[sk->perm[i]]);
	}

}

void presign(f3_vector *statement, f3_vector *signature, f3_vector *message_hash, const uint8_t seed_salt, const uint8_t *message, const size_t mlen, wave_sk_t *sk, mf3 *MM) {
	f3_vector test1 = f3_vector_new(N-K);
	f3_vector test2 = f3_vector_new(N-K);
	f3_vector test3 = f3_vector_new(N - K);
	f3_vector test4 = f3_vector_new(K);
	uint8_t * aa= (uint8_t*) malloc((N-K)* sizeof(uint8_t));
	uint8_t salt[SALT_SIZE] = { 0 };
	prng_t *PRNG1 = prng_init(seed_salt);
	f3_vector r = f3_vector_new(N);
	f3_vector_rand(&r, PRNG1);
	for (size_t b = 0; b < N-K; b++){
		uint8_t bb=f3_vector_get_coeff(&r, b);
		f3_vector_setcoeff(&test1, b, bb);
		}
	for (size_t b = 0; b < K; b++){
		uint8_t bb=f3_vector_get_coeff(&r, N-K+b);
		f3_vector_setcoeff(&test4, b, bb);
	}
	mf3_mv_mul(mf3_transpose(MM), &test4, aa);
	f3_vector_set_from_array(&test2,aa,N-K);
	f3_vector_add(&test1, &test2, &test3);
	//printf("\n   Presign   \n ");
	
	f3_vector_add(&test3, statement, &test1);
	//f3_vector_print_big(&test1);
	for (size_t b = 0; b < N-K; b++){
		salt[b]=f3_vector_get_coeff(&test1, b);
	}
	//printf("\n======= Presign %u ========\n", seed_salt);



	//randombytes(salt, SALT_SIZE);
	uint8_t seed;
	randombytes(&seed, 1);
	hash_message(message_hash, message, mlen, salt, SALT_SIZE);
	prng_t *PRNG = prng_init(seed);
	sign_wave(signature, message_hash, sk, PRNG);
	prng_clear(PRNG);
	f3_vector_free(&test1);
	f3_vector_free(&test2);
	f3_vector_free(&test3);
	f3_vector_free(&test4);
	f3_vector_free(&r);
	prng_clear(PRNG1);
}







void sign(f3_vector *signature, f3_vector *message_hash, const uint8_t seed_salt, const uint8_t *message, const size_t mlen, wave_sk_t *sk, mf3 *MM) {
		f3_vector test1 = f3_vector_new(N-K);
	f3_vector test2 = f3_vector_new(N-K);
	f3_vector test3 = f3_vector_new(N - K);
	f3_vector test4 = f3_vector_new(K);
	uint8_t * aa= (uint8_t*) malloc((N-K)* sizeof(uint8_t));
	uint8_t salt[SALT_SIZE] = { 0 };
	prng_t *PRNG1 = prng_init(seed_salt);
	f3_vector r = f3_vector_new(N);
	f3_vector_rand(&r, PRNG1);
	for (size_t b = 0; b < N-K; b++){
		uint8_t bb=f3_vector_get_coeff(&r, b);
		f3_vector_setcoeff(&test1, b, bb);
		}
	for (size_t b = 0; b < K; b++){
		uint8_t bb=f3_vector_get_coeff(&r, N-K+b);
		f3_vector_setcoeff(&test4, b, bb);
	}
	mf3_mv_mul(mf3_transpose(MM), &test4, aa);
	f3_vector_set_from_array(&test2,aa,N-K);
	f3_vector_add(&test1, &test2, &test3);
	//printf("\n   Presign   \n ");
	
	//f3_vector_add(&test3, statement, &test1);
	//f3_vector_print_big(&test1);
	for (size_t b = 0; b < N-K; b++){
		salt[b]=f3_vector_get_coeff(&test3, b);
	}
	//printf("\n======= Presign %u ========\n", seed_salt);



	//randombytes(salt, SALT_SIZE);
	uint8_t seed;
	randombytes(&seed, 1);
	hash_message(message_hash, message, mlen, salt, SALT_SIZE);
	prng_t *PRNG = prng_init(seed);
	sign_wave(signature, message_hash, sk, PRNG);
	prng_clear(PRNG);
	f3_vector_free(&test1);
	f3_vector_free(&test2);
	f3_vector_free(&test3);
	f3_vector_free(&test4);
	f3_vector_free(&r);
	prng_clear(PRNG1);
}
