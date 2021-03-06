--- vncstorepw/ultravnc_dsm_helper.c
+++ vncstorepw/ultravnc_dsm_helper.c
@@ -414,7 +414,9 @@ void enc_do(char *ciph, char *keyfile, c
 		if (strstr(p, "md5+") == p) {
 			Digest = EVP_md5();        p += strlen("md5+");
 		} else if (strstr(p, "sha+") == p) {
-			Digest = EVP_sha();        p += strlen("sha+");
+			fprintf(stderr, "%s: obsolete hash algorithm: SHA-0\n",
+			    prog, s);
+			exit(1);
 		} else if (strstr(p, "sha1+") == p) {
 			Digest = EVP_sha1();       p += strlen("sha1+");
 		} else if (strstr(p, "ripe+") == p) {
@@ -655,8 +657,10 @@ static void enc_xfer(int sock_fr, int so
 	 */
 	unsigned char E_keystr[EVP_MAX_KEY_LENGTH];
 	unsigned char D_keystr[EVP_MAX_KEY_LENGTH];
-	EVP_CIPHER_CTX E_ctx, D_ctx;
-	EVP_CIPHER_CTX *ctx = NULL;
+	//openssl1.1.patch - Do NOT create two context and only use one
+	// - that's silly.
+	//EVP_CIPHER_CTX *E_ctx, *D_ctx;
+	EVP_CIPHER_CTX *ctx;
 
 	unsigned char buf[BSIZE], out[BSIZE];
 	unsigned char *psrc = NULL, *keystr;
@@ -698,11 +702,14 @@ static void enc_xfer(int sock_fr, int so
 	encsym = encrypt ? "+" : "-";
 
 	/* use the encryption/decryption context variables below */
+	ctx = EVP_CIPHER_CTX_new();
+	if (!ctx) {
+	    fprintf(stderr, "Failed to create encryption/decryption context.\n");
+	    goto finished;
+	}
 	if (encrypt) {
-		ctx = &E_ctx;
 		keystr = E_keystr;
 	} else {
-		ctx = &D_ctx;
 		keystr = D_keystr;
 	}
 
@@ -797,7 +804,6 @@ static void enc_xfer(int sock_fr, int so
 		if (whoops) {
 			fprintf(stderr, "%s: %s - WARNING: MSRC4 mode and IGNORING random salt\n", prog, encstr);
 			fprintf(stderr, "%s: %s - WARNING: and initialization vector!!\n", prog, encstr);
-			EVP_CIPHER_CTX_init(ctx);
 			if (pw_in) {
 			    /* for pw=xxxx a md5 hash is used */
 			    EVP_BytesToKey(Cipher, Digest, NULL, (unsigned char *) keydata,
@@ -816,7 +822,6 @@ static void enc_xfer(int sock_fr, int so
 
 			EVP_BytesToKey(Cipher, Digest, NULL, (unsigned char *) keydata,
 			    keydata_len, 1, keystr, ivec); 
-			EVP_CIPHER_CTX_init(ctx);
 			EVP_CipherInit_ex(ctx, Cipher, NULL, keystr, ivec,
 			    encrypt);
 		}
@@ -836,9 +841,9 @@ static void enc_xfer(int sock_fr, int so
 			in_salt = salt;
 		}
 
-		if (ivec_size < Cipher->iv_len && !securevnc) {
+		if (ivec_size < EVP_CIPHER_iv_length(Cipher) && !securevnc) {
 			fprintf(stderr, "%s: %s - WARNING: short IV %d < %d\n",
-			    prog, encstr, ivec_size, Cipher->iv_len);
+			    prog, encstr, ivec_size, EVP_CIPHER_iv_length(Cipher));
 		}
 
 		/* make the hashed value and place in keystr */
@@ -877,9 +882,6 @@ static void enc_xfer(int sock_fr, int so
 		}
 
 
-		/* initialize the context */
-		EVP_CIPHER_CTX_init(ctx);
-
 
 		/* set the cipher & initialize */
 
@@ -986,6 +988,7 @@ static void enc_xfer(int sock_fr, int so
 	/* transfer done (viewer exited or some error) */
 	finished:
 
+	if (ctx) EVP_CIPHER_CTX_free(ctx);
 	fprintf(stderr, "\n%s: %s - close sock_to\n", prog, encstr);
 	close(sock_to);
 
@@ -1060,14 +1063,14 @@ static int securevnc_server_rsa_save_dia
 }
 
 static char *rsa_md5_sum(unsigned char* rsabuf) {
-	EVP_MD_CTX md;
+	EVP_MD_CTX *md = EVP_MD_CTX_create();
 	char digest[EVP_MAX_MD_SIZE], tmp[16];
 	char md5str[EVP_MAX_MD_SIZE * 8];
 	unsigned int i, size = 0;
 
-	EVP_DigestInit(&md, EVP_md5());
-	EVP_DigestUpdate(&md, rsabuf, SECUREVNC_RSA_PUBKEY_SIZE);
-	EVP_DigestFinal(&md, (unsigned char *)digest, &size);
+	EVP_DigestInit(md, EVP_md5());
+	EVP_DigestUpdate(md, rsabuf, SECUREVNC_RSA_PUBKEY_SIZE);
+	EVP_DigestFinal(md, (unsigned char *)digest, &size);
 
 	memset(md5str, 0, sizeof(md5str));
 	for (i=0; i < size; i++) {
@@ -1075,6 +1078,7 @@ static char *rsa_md5_sum(unsigned char*
 		sprintf(tmp, "%02x", (int) uc);
 		strcat(md5str, tmp);
 	}
+	EVP_MD_CTX_destroy(md);
 	return strdup(md5str);
 }
 
@@ -1184,7 +1188,7 @@ static void sslexit(char *msg) {
 
 static void securevnc_setup(int conn1, int conn2) {
 	RSA *rsa = NULL;
-	EVP_CIPHER_CTX init_ctx;
+	EVP_CIPHER_CTX *init_ctx = EVP_CIPHER_CTX_new();
 	unsigned char keystr[EVP_MAX_KEY_LENGTH];
 	unsigned char *rsabuf, *rsasav;
 	unsigned char *encrypted_keybuf;
@@ -1203,6 +1207,8 @@ static void securevnc_setup(int conn1, i
 
 	ERR_load_crypto_strings();
 
+	if (!init_ctx) sslexit("securevnc_setup: EVP_CIPHER_CTX_new() failed");
+	
 	/* alloc and read from server the 270 comprising the rsa public key: */
 	rsabuf = (unsigned char *) calloc(SECUREVNC_RSA_PUBKEY_SIZE, 1);
 	rsasav = (unsigned char *) calloc(SECUREVNC_RSA_PUBKEY_SIZE, 1);
@@ -1323,8 +1329,7 @@ static void securevnc_setup(int conn1, i
 	/*
 	 * Back to the work involving the tmp obscuring key:
 	 */
-	EVP_CIPHER_CTX_init(&init_ctx);
-	rc = EVP_CipherInit_ex(&init_ctx, EVP_rc4(), NULL, initkey, NULL, 1);
+	rc = EVP_CipherInit_ex(init_ctx, EVP_rc4(), NULL, initkey, NULL, 1);
 	if (rc == 0) {
 		sslexit("securevnc_setup: EVP_CipherInit_ex(init_ctx) failed");
 	}
@@ -1340,13 +1345,13 @@ static void securevnc_setup(int conn1, i
 	/* decode with the tmp key */
 	if (n > 0) {
 		memset(to_viewer, 0, sizeof(to_viewer));
-		if (EVP_CipherUpdate(&init_ctx, to_viewer, &len, buf, n) == 0) {
+		if (EVP_CipherUpdate(init_ctx, to_viewer, &len, buf, n) == 0) {
 			sslexit("securevnc_setup: EVP_CipherUpdate(init_ctx) failed");
 			exit(1);
 		}
 		to_viewer_len = len;
 	}
-	EVP_CIPHER_CTX_cleanup(&init_ctx);
+	EVP_CIPHER_CTX_free(init_ctx);
 	free(initkey);
 
 	/* print what we would send to the viewer (sent below): */
@@ -1407,7 +1412,7 @@ static void securevnc_setup(int conn1, i
 
 	if (client_auth_req && client_auth) {
 		RSA *client_rsa = load_client_auth(client_auth);
-		EVP_MD_CTX dctx;
+		EVP_MD_CTX *dctx = EVP_MD_CTX_create();
 		unsigned char digest[EVP_MAX_MD_SIZE], *signature;
 		unsigned int ndig = 0, nsig = 0;
 
@@ -1421,8 +1426,8 @@ static void securevnc_setup(int conn1, i
 			exit(1);
 		}
 
-		EVP_DigestInit(&dctx, EVP_sha1());
-		EVP_DigestUpdate(&dctx, keystr, SECUREVNC_KEY_SIZE);
+		EVP_DigestInit(dctx, EVP_sha1());
+		EVP_DigestUpdate(dctx, keystr, SECUREVNC_KEY_SIZE);
 		/*
 		 * Without something like the following MITM is still possible.
 		 * This is because the MITM knows keystr and can use it with
@@ -1433,7 +1438,7 @@ static void securevnc_setup(int conn1, i
 		 * he doesn't have Viewer_ClientAuth.pkey.
 		 */
 		if (0) {
-			EVP_DigestUpdate(&dctx, rsasav, SECUREVNC_RSA_PUBKEY_SIZE);
+			EVP_DigestUpdate(dctx, rsasav, SECUREVNC_RSA_PUBKEY_SIZE);
 			if (!keystore_verified) {
 				fprintf(stderr, "securevnc_setup:\n");
 				fprintf(stderr, "securevnc_setup: Warning: even *WITH* Client Authentication in SecureVNC,\n");
@@ -1456,7 +1461,8 @@ static void securevnc_setup(int conn1, i
 				fprintf(stderr, "securevnc_setup:\n");
 			}
 		}
-		EVP_DigestFinal(&dctx, (unsigned char *)digest, &ndig);
+		EVP_DigestFinal(dctx, (unsigned char *)digest, &ndig);
+		EVP_MD_CTX_destroy(dctx);
 
 		signature = (unsigned char *) calloc(RSA_size(client_rsa), 1);
 		RSA_sign(NID_sha1, digest, ndig, signature, &nsig, client_rsa);
