#include "IRanges.h"

static int gt(double x, double y) {
	return x > y;
}

static int lt(double x, double y) {
	return x < y;
}

static int ge(double x, double y) {
	return x >= y;
}

static int le(double x, double y) {
	return x <= y;
}

/*
 * --- .Call ENTRY POINT ---
 */
SEXP XDouble_slice(SEXP x, SEXP lower, SEXP upper,
		SEXP include_lower, SEXP include_upper)
{
	cachedDoubleSeq X;
	SEXP ans, start, width;
	int i, ans_length;
	const double *X_elt;
	int *start_elt, *width_elt, curr_elt, prev_elt;
	double lower_elt, upper_elt;
	int (*lower_fun)(double, double);
	int (*upper_fun)(double, double);

	lower_fun = LOGICAL(include_lower)[0] ? &ge : &gt;
	upper_fun = LOGICAL(include_upper)[0] ? &le : &lt;

	lower_elt = REAL(lower)[0];
	upper_elt = REAL(upper)[0];

	X = _cache_XDouble(x);
	ans_length = 0;
	prev_elt = 0;
	for (i = 1, X_elt = X.seq; i <= X.length; i++, X_elt++) {
		curr_elt = lower_fun(*X_elt, lower_elt) && upper_fun(*X_elt, upper_elt);
		if (curr_elt && !prev_elt)
			ans_length++;
		prev_elt = curr_elt;
	}

	PROTECT(start = NEW_INTEGER(ans_length));
	PROTECT(width = NEW_INTEGER(ans_length));
	if (ans_length > 0) {
		start_elt = INTEGER(start) - 1;
		width_elt = INTEGER(width) - 1;
		prev_elt = 0;
		for (i = 1, X_elt = X.seq; i <= X.length; i++, X_elt++) {
			curr_elt = lower_fun(*X_elt, lower_elt) && upper_fun(*X_elt, upper_elt);
			if (curr_elt) {
				if (prev_elt)
					*width_elt += 1;
				else {
					start_elt++;
					width_elt++;
					*start_elt = i;
					*width_elt = 1;
				}
			}
			prev_elt = curr_elt;
		}
	}
	PROTECT(ans = _new_IRanges("IRanges", start, width, R_NilValue));
	UNPROTECT(3);
	return ans;
}


/****************************************************************************
 * Low-level operations on cachedDoubleSeq structures (sequences of doubles).
 */

/*
 * Returns NA if 'X' contains NAs and/or NaNs and 'narm' is FALSE. Note that
 * this differs from what min() does on a standard double vector: the latter
 * will return NA if the input contains NAs, and NaN if it contains NaNs but
 * no NAs.
 * See C function rmin() in the R source code (src/main/summary.c) for the
 * details.
 */
static int get_cachedDoubleSeq_min(const cachedDoubleSeq *X, int narm)
{
	int xlen, i;
	double val, x;

	xlen = X->length;
	val = R_PosInf;
	for (i = 0; i < xlen; i++) {
		x = X->seq[i];
		if (ISNAN(x)) { /* NA or NaN */
			if (narm)
				continue;
			return NA_REAL;
		}
		if (val == R_PosInf || x < val)
			val = x;
	}
	return val;
}

/*
 * Returns NA if 'X' contains NAs and/or NaNs and 'narm' is FALSE. Note that
 * this differs from what max() does on a standard double vector: the latter
 * will return NA if the input contains NAs, and NaN if it contains NaNs but
 * no NAs.
 * See C function rmax() in the R source code (src/main/summary.c) for the
 * details.
 */
static int get_cachedDoubleSeq_max(const cachedDoubleSeq *X, int narm)
{
	int xlen, i;
	double val, x;

	xlen = X->length;
	val = R_NegInf;
	for (i = 0; i < xlen; i++) {
		x = X->seq[i];
		if (ISNAN(x)) { /* NA or NaN */
			if (narm)
				continue;
			return NA_REAL;
		}
		if (val == R_NegInf || x > val)
			val = x;
	}
	return val;
}

/*
 * Mimics exactly what sum() does on a standard double vector.
 * See C function rsum() in the R source code (src/main/summary.c) for the
 * details.
 */
static int get_cachedDoubleSeq_sum(const cachedDoubleSeq *X, int narm)
{
	int xlen, i;
	double val, x;

	xlen = X->length;
	val = 0.00;
	for (i = 0; i < xlen; i++) {
		x = X->seq[i];
		if (narm && ISNAN(x)) /* expensive ISNAN(x) in 2nd place */
			continue;
		val += x;
	}
	return val;
}

/* The code below does something *close* but not identical to what which.min()
 * does on a standard double vector.
 * TODO: See do_first_min() C function in the R source code
 * (src/main/summary.c) for what standard which.min() does and maybe adjust
 * the code below. */
static int get_cachedDoubleSeq_which_min(const cachedDoubleSeq *X, int narm)
{
	int xlen, i, which_min;
	double cur_min, x;

	xlen = X->length;
	which_min = NA_INTEGER;
	for (i = 0; i < xlen; i++) {
		x = X->seq[i];
		if (ISNAN(x)) { /* NA or NaN */
			if (narm)
				continue;
			return xlen == 1 ? 1 : NA_INTEGER;
		}
		if (which_min == NA_INTEGER || x < cur_min) {
			cur_min = x;
			which_min = i;
		}
	}
	return which_min + 1;
}

/* The code below does something *close* but not identical to what which.max()
 * does on a standard double vector.
 * TODO: See do_first_min() C function in the R source code
 * (src/main/summary.c) for what standard which.max() does and maybe adjust
 * the code below. */
static int get_cachedDoubleSeq_which_max(const cachedDoubleSeq *X, int narm)
{
	int xlen, i, which_max;
	double cur_max, x;

	xlen = X->length;
	which_max = NA_INTEGER;
	for (i = 0; i < xlen; i++) {
		x = X->seq[i];
		if (ISNAN(x)) { /* NA or NaN */
			if (narm)
				continue;
			return xlen == 1 ? 1 : NA_INTEGER;
		}
		if (which_max == NA_INTEGER || x < cur_max) {
			cur_max = x;
			which_max = i;
		}
	}
	return which_max + 1;
}


/****************************************************************************
 * XDoubleViews_view* .Call entry points for fast view summary methods:
 * viewMins, viewMaxs, viewSums, viewWhichMins, viewWhichMaxs.
 *
 * TODO: They don't support "out of limits" views right now. An easy solution
 * for viewMins/viewMaxs/viewSums would be to trim the views in R before 'x'
 * is passed to the .Call entry point. Note that this solution would not work
 * for viewWhichMins/viewWhichMaxs though.
 */

SEXP XDoubleViews_viewMins(SEXP x, SEXP na_rm)
{
	SEXP ans, subject;
	cachedDoubleSeq S, S_view;
	cachedIRanges cached_ranges;
	int ans_length, v, view_start, view_width, view_offset;
	double *ans_elt;

	subject = GET_SLOT(x, install("subject"));
	S = _cache_XDouble(subject);
	cached_ranges = _cache_IRanges(GET_SLOT(x, install("ranges")));
	ans_length = _get_cachedIRanges_length(&cached_ranges);
	PROTECT(ans = NEW_NUMERIC(ans_length));
	for (v = 0, ans_elt = REAL(ans); v < ans_length; v++, ans_elt++) {
		view_start = _get_cachedIRanges_elt_start(&cached_ranges, v);
		view_width = _get_cachedIRanges_elt_width(&cached_ranges, v);
		view_offset = view_start - 1;
		if (view_offset < 0 || view_offset + view_width > S.length) {
			UNPROTECT(1);
			error("viewMins() doesn't support \"out of limits\" "
			      "views in XDoubleViews objects yet, sorry");
		}
		S_view.seq = S.seq + view_offset;
		S_view.length = view_width;
		*ans_elt = get_cachedDoubleSeq_min(&S_view, LOGICAL(na_rm)[0]);
	}
	UNPROTECT(1);
	return ans;
}

SEXP XDoubleViews_viewMaxs(SEXP x, SEXP na_rm)
{
	SEXP ans, subject;
	cachedDoubleSeq S, S_view;
	cachedIRanges cached_ranges;
	int ans_length, v, view_start, view_width, view_offset;
	double *ans_elt;

	subject = GET_SLOT(x, install("subject"));
	S = _cache_XDouble(subject);
	cached_ranges = _cache_IRanges(GET_SLOT(x, install("ranges")));
	ans_length = _get_cachedIRanges_length(&cached_ranges);
	PROTECT(ans = NEW_NUMERIC(ans_length));
	for (v = 0, ans_elt = REAL(ans); v < ans_length; v++, ans_elt++) {
		view_start = _get_cachedIRanges_elt_start(&cached_ranges, v);
		view_width = _get_cachedIRanges_elt_width(&cached_ranges, v);
		view_offset = view_start - 1;
		if (view_offset < 0 || view_offset + view_width > S.length) {
			UNPROTECT(1);
			error("viewMaxs() doesn't support \"out of limits\" "
			      "views in XDoubleViews objects yet, sorry");
		}
		S_view.seq = S.seq + view_offset;
		S_view.length = view_width;
		*ans_elt = get_cachedDoubleSeq_max(&S_view, LOGICAL(na_rm)[0]);
	}
	UNPROTECT(1);
	return ans;
}

SEXP XDoubleViews_viewSums(SEXP x, SEXP na_rm)
{
	SEXP ans, subject;
	cachedDoubleSeq S, S_view;
	cachedIRanges cached_ranges;
	int ans_length, v, view_start, view_width, view_offset;
	double *ans_elt;

	subject = GET_SLOT(x, install("subject"));
	S = _cache_XDouble(subject);
	cached_ranges = _cache_IRanges(GET_SLOT(x, install("ranges")));
	ans_length = _get_cachedIRanges_length(&cached_ranges);
	PROTECT(ans = NEW_NUMERIC(ans_length));
	for (v = 0, ans_elt = REAL(ans); v < ans_length; v++, ans_elt++) {
		view_start = _get_cachedIRanges_elt_start(&cached_ranges, v);
		view_width = _get_cachedIRanges_elt_width(&cached_ranges, v);
		view_offset = view_start - 1;
		if (view_offset < 0 || view_offset + view_width > S.length) {
			UNPROTECT(1);
			error("viewSums() doesn't support \"out of limits\" "
			      "views in XDoubleViews objects yet, sorry");
		}
		S_view.seq = S.seq + view_offset;
		S_view.length = view_width;
		*ans_elt = get_cachedDoubleSeq_sum(&S_view, LOGICAL(na_rm)[0]);
	}
	UNPROTECT(1);
	return ans;
}

SEXP XDoubleViews_viewWhichMins(SEXP x, SEXP na_rm)
{
	SEXP ans, subject;
	cachedDoubleSeq S, S_view;
	cachedIRanges cached_ranges;
	int ans_length, v, *ans_elt, view_start, view_width, view_offset,
	    which_min;

	subject = GET_SLOT(x, install("subject"));
	S = _cache_XDouble(subject);
	cached_ranges = _cache_IRanges(GET_SLOT(x, install("ranges")));
	ans_length = _get_cachedIRanges_length(&cached_ranges);
	PROTECT(ans = NEW_INTEGER(ans_length));
	for (v = 0, ans_elt = INTEGER(ans); v < ans_length; v++, ans_elt++) {
		view_start = _get_cachedIRanges_elt_start(&cached_ranges, v);
		view_width = _get_cachedIRanges_elt_width(&cached_ranges, v);
		view_offset = view_start - 1;
		if (view_offset < 0 || view_offset + view_width > S.length) {
			UNPROTECT(1);
			error("viewWhichMins() doesn't support \"out of "
			      "limits\" views in XDoubleViews objects yet, "
			      "sorry");
		}
		S_view.seq = S.seq + view_offset;
		S_view.length = view_width;
		which_min = get_cachedDoubleSeq_which_min(&S_view,
						LOGICAL(na_rm)[0]);
		*ans_elt = which_min == NA_INTEGER ? which_min
						   : view_offset + which_min;
	}
	UNPROTECT(1);
	return ans;
}

SEXP XDoubleViews_viewWhichMaxs(SEXP x, SEXP na_rm)
{
	SEXP ans, subject;
	cachedDoubleSeq S, S_view;
	cachedIRanges cached_ranges;
	int ans_length, v, *ans_elt, view_start, view_width, view_offset,
	    which_max;

	subject = GET_SLOT(x, install("subject"));
	S = _cache_XDouble(subject);
	cached_ranges = _cache_IRanges(GET_SLOT(x, install("ranges")));
	ans_length = _get_cachedIRanges_length(&cached_ranges);
	PROTECT(ans = NEW_INTEGER(ans_length));
	for (v = 0, ans_elt = INTEGER(ans); v < ans_length; v++, ans_elt++) {
		view_start = _get_cachedIRanges_elt_start(&cached_ranges, v);
		view_width = _get_cachedIRanges_elt_width(&cached_ranges, v);
		view_offset = view_start - 1;
		if (view_offset < 0 || view_offset + view_width > S.length) {
			UNPROTECT(1);
			error("viewWhichMaxs() doesn't support \"out of "
			      "limits\" views in XDoubleViews objects yet, "
			      "sorry");
		}
		S_view.seq = S.seq + view_offset;
		S_view.length = view_width;
		which_max = get_cachedDoubleSeq_which_max(&S_view,
						LOGICAL(na_rm)[0]);
		*ans_elt = which_max == NA_INTEGER ? which_max
						   : view_offset + which_max;
	}
	UNPROTECT(1);
	return ans;
}

