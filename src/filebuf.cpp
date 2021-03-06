#include "../include/filebuf.hpp"
#include "../include/util.hpp"
#if !defined CSDBG_WITH_PLUGIN && !defined CSDBG_WITH_HIGHLIGHT
#include "../include/exception.hpp"
#endif

/**
	@file src/filebuf.cpp

	@brief Class csdbg::filebuf method implementation
*/

namespace csdbg {

/**
 * @brief Object constructor
 *
 * @param[in] path the output file path
 *
 * @throws std::bad_alloc
 * @throws csdbg::exception
 */
filebuf::filebuf(const i8 *path)
try:
streambuf(),
m_path(NULL)
{
	if ( unlikely(path == NULL) )
		throw exception("invalid argument: path (=%p)", path);

	m_path = new i8[strlen(path) + 1];
	strcpy(m_path, path);
}

catch (...) {
	delete[] m_data;
	m_data = NULL;
	m_path = NULL;
}


/**
 * @brief Object copy constructor
 *
 * @param[in] src the source object
 *
 * @throws std::bad_alloc
 * @throws csdbg::exception
 */
filebuf::filebuf(const filebuf &src)
try:
streambuf(src),
m_path(NULL)
{
	m_path = new i8[strlen(src.m_path) + 1];
	strcpy(m_path, src.m_path);
}

catch (...) {
	delete[] m_data;
	m_data = NULL;
	m_path = NULL;
	close();
}


/**
 * @brief Object destructor
 */
filebuf::~filebuf()
{
	delete[] m_path;
	m_path = NULL;
}


/**
 * @brief Object virtual copy constructor
 *
 * @returns the object copy (heap allocated)
 *
 * @throws std::bad_alloc
 * @throws csdbg::exception
 */
inline filebuf* filebuf::clone() const
{
	return new filebuf(*this);
}


/**
 * @brief Get the output file path
 *
 * @returns this->m_path
 */
inline const i8* filebuf::path() const
{
	return m_path;
}


/**
 * @brief Assignment operator
 *
 * @param[in] rval the assigned object
 *
 * @returns *this
 *
 * @throws std::bad_alloc
 * @throws csdbg::exception
 */
filebuf& filebuf::operator=(const filebuf &rval)
{
	if ( unlikely(this == &rval) )
		return *this;

	/* Copy the buffer and duplicate the stream descriptor */
	streambuf::operator=(rval);

	u32 len = strlen(rval.m_path);
	if (len > strlen(m_path)) {
		delete[] m_path;
		m_path = NULL;
		m_path = new i8[len + 1];
	}

	strcpy(m_path, rval.m_path);
	return *this;
}


/**
 * @brief Open the file for output
 *
 * @returns *this
 *
 * @throws csdbg::exception
 *
 * @note
 *	If the file doesn't exist it is created, if it exists it is opened in append
 *	mode
 */
inline filebuf& filebuf::open()
{
	return open(O_WRONLY | O_CREAT | O_APPEND, 0644);
}


/**
 * @brief Open the file
 *
 * @param[in] flags the flags used to open the file
 *
 * @param[in] umask the file mode (ignored if the file exists)
 *
 * @returns *this
 *
 * @throws csdbg::exception
 *
 * @note If the file is already open, it is re-opened with the new flags
 */
filebuf& filebuf::open(u32 flags, u32 umask)
{
	if ( unlikely(m_handle >= 0) )
		close();

	do {
		m_handle = ::open(m_path, flags, umask);
	}
	while ( unlikely(m_handle < 0 && errno == EINTR) );

	if ( unlikely(m_handle < 0) )
		throw exception(
			"failed to open file '%s' (errno %d - %s)",
			m_path,
			errno,
			strerror(errno)
		);

	/* Sanity checks, if a test fails an appropriate exception is thrown */
	try {
		fileinfo_t inf;
		i32 retval = fstat(m_handle, &inf);
		if ( unlikely(retval < 0) )
			throw exception(
				"failed to stat path '%s' (errno %d - %s)",
				m_path,
				errno,
				strerror(errno)
			);

		else if ( unlikely(!util::is_regular(inf)) )
			throw exception("'%s' is not a regular file", m_path);

		else if ( unlikely(!util::is_writable(inf)) )
			throw exception("file '%s' is not writable", m_path);

		return *this;
	}

	catch (...) {
		/* If any of the checks failed, the descriptor is unusable */
		close();
		throw;
	}
}


/**
 * @brief Flush the buffered data to the file
 *
 * @returns *this
 *
 * @throws csdbg::exception
 */
filebuf& filebuf::flush()
{
	try {
		streambuf::flush();
		return *this;
	}

	catch (i32 err) {
		throw exception(
			"failed to write data to file '%s' (errno %d - %s)",
			m_path,
			err,
			strerror(err)
		);
	}
}


/**
 * @brief Commit cached data to the file
 *
 * @returns *this
 *
 * @throws csdbg::exception
 */
inline filebuf& filebuf::sync() const
{
	return sync(false);
}


/**
 * @brief Commit cached data to the file
 *
 * @param[in] full false to perform a data sync (omit file metadata)
 *
 * @returns *this
 *
 * @throws csdbg::exception
 */
filebuf& filebuf::sync(bool full) const
{
	i32 retval;
	if ( likely(full) )
		retval = fsync(m_handle);
	else
		retval = fdatasync(m_handle);

	if ( unlikely(retval < 0) )
		throw exception("failed to sync file '%s'", m_path);

	return const_cast<filebuf&> (*this);
}


/**
 * @brief Seek the file pointer to a new position
 *
 * @param[in] offset the new position
 *
 * @param[in] rel true to seek relatively to the current position
 *
 * @returns *this
 *
 * @throws csdbg::exception
 */
filebuf& filebuf::seek_to(i32 offset, bool rel)
{
	i32 whence = (rel) ? SEEK_CUR : SEEK_SET;
	i32 retval = lseek(m_handle, offset, whence);
	if ( unlikely(retval < 0) )
		throw exception(
			"failed to seek file '%s' to offset %d (errno %d - %s)",
			m_path,
			offset,
			errno,
			strerror(errno)
		);

	return *this;
}


/**
 * @brief Resize the file
 *
 * @param[in] sz the new size
 *
 * @returns *this
 *
 * @throws csdbg::exception
 *
 * @note
 *	If the file was larger than 'sz', the extra data is lost. If the file was
 *	shorter, it is extended, and the extended part reads as null bytes ('\0').
 *	The file offset is not changed
 */
filebuf& filebuf::resize(u32 sz)
{
	i32 retval;
	do {
		retval = ftruncate(m_handle, sz);
	}
	while ( unlikely(retval < 0 && errno == EINTR) );

	if ( unlikely(retval < 0) )
		throw errno;

	return *this;
}


/**
 * @brief
 *	Create a unique ID based on process identifiers arranged as indicated by a
 *	printf-style format string
 *
 * @param[in] fmt the format string
 *
 * @returns the ID (heap allocated)
 *
 * @throws std::bad_alloc
 * @throws csdbg::exception
 *
 * @note
 *	The ID can be used to name files in an unambiguous way. The specifiers are:
 *	<br><br>
 *	<ul>
 *		<li>%%e - executable name
 *		<li>%%a - executable absolute path
 *		<li>%%p - process ID
 *		<li>%%t - thread ID
 *		<li>%%s - timestamp (in microseconds)
 *	</ul><br>
 *
 * @attention
 *	If fmt is NULL or an empty string then the default format %%e_%%p_%%t_%%s is
 *	used. All numeric values are hexadecimal
 */
string* filebuf::unique_id(const i8 *fmt)
{
	__D_ASSERT(fmt != NULL);
	if ( unlikely(fmt == NULL || strlen(fmt) == 0) )
		fmt = "%e_%p_%t_%s";

	struct timeval now;
	gettimeofday(&now, NULL);
	u64 tstamp = static_cast<u64> (now.tv_sec) * 10e+5 + now.tv_usec;

	const i8 *path = util::exec_path();
	string *retval = NULL;

	/* If an exception occurs, release resources and rethrow it */
	try {
		retval = new string();

		for (u32 i = 0, len = strlen(fmt); likely(i < len); i++) {
			i8 ch = fmt[i];
			if ( likely(ch != '%') ) {
				retval->append(ch);
				continue;
			}

			if ( unlikely(i == len - 1) )
				throw exception("invalid format '%s' (at %d: no specifier)", fmt, ++i);

			ch = fmt[++i];
			switch (ch) {
			case '%':
				retval->append(ch);
				break;

			case 'a':
				retval->append(path);
				break;

			case 'e':
				retval->append(basename(path));
				break;

			case 'p':
				retval->append("%x", getpid());
				break;

			case 't':
				retval->append("%lx", pthread_self());
				break;

			case 's':
				retval->append("%lx", tstamp);
				break;

			default: {
				const i8 err[] = "invalid format '%s' (at %d: unknown specifier '%c')";
				throw exception(err, fmt, i, ch);
			}}
		}

		delete[] path;
		return retval;
	}

	catch (...) {
		delete[] path;
		delete retval;
		throw;
	}
}

}

