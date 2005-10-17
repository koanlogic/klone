/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

int dummy = 0;

/* 
 * A 'file' log is physically subdivided in a certain number of files (pages)
 * named "<basename>.<page_id>" used as a sliding circular buffer.
 * A page must be thought as a fixed size array of log lines.  Each page 
 * in a 'file' log is of the same dimension so that each log line can be
 * referenced univocally.  Suppose a 'file' log made of n pages p_0, p_1, 
 * ..., p_n-1 of size m: the i-th line (0 <= i < n*m) will be found in page 
 * p_i%m at offset i%n.  We assume that at least 2 pages (n=2) exist.
 *
 * State informations are grouped into a 'head' file to be preserved between
 * one run and the subsequent.  Informations in 'head' (i.e. n, m, active page
 * id, offset in it) are used iff they correspond to actual config parameters.
 * Otherwise past log is discarded.
 *
 * (1) initialisation (where to start writing)
 * (2) append a log line
 * (3) retrieve the nth log line
 * (4) clear all
 * (5) termination (flush state to disk on exit)
 *
 * (1)
 * The 'file' log initialisation phase consists in the selection of an 
 * available page and an offset in it, where to start appending log messages.
 * The needed informations, if consistent with the supplied conf parameters, 
 * are gathered from the 'head' page.  If no 'head' is available (non-existent,
 * damaged or inconsistent with conf) the write pointer will be placed at page
 * zero, offset zero.
 */

