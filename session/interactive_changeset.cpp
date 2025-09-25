// interactive_changeset.cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::vector;

// ---- minimal sqlite3session.h shim ----
extern "C" {
struct sqlite3_changeset_iter;
int sqlite3changeset_apply(sqlite3 *, int, const void *,
                           int (*)(void *, const char *),
                           int (*)(void *, int, sqlite3_changeset_iter *),
                           void *);
int sqlite3changeset_op(sqlite3_changeset_iter *, const char **, int *, int *,
                        int *);
int sqlite3changeset_pk(sqlite3_changeset_iter *, unsigned char **);
int sqlite3changeset_old(sqlite3_changeset_iter *, int, sqlite3_value **);
int sqlite3changeset_new(sqlite3_changeset_iter *, int, sqlite3_value **);
int sqlite3changeset_conflict(sqlite3_changeset_iter *, int, sqlite3_value **);
}
#ifndef SQLITE_CHANGESET_OMIT
#define SQLITE_CHANGESET_OMIT 0
#define SQLITE_CHANGESET_REPLACE 1
#define SQLITE_CHANGESET_ABORT 2
#endif
#ifndef SQLITE_CHANGESET_DATA
#define SQLITE_CHANGESET_DATA 1
#define SQLITE_CHANGESET_NOTFOUND 2
#define SQLITE_CHANGESET_CONFLICT 3
#define SQLITE_CHANGESET_CONSTRAINT 4
#define SQLITE_CHANGESET_FOREIGN_KEY 5
#endif
// ---------------------------------------

struct Ctx {
  sqlite3 *db{};
  bool dryRun{};
  std::unordered_map<string, vector<string>> colsCache;
};

static string v2s(sqlite3_value *v) {
  if (!v)
    return "-";
  switch (sqlite3_value_type(v)) {
  case SQLITE_INTEGER:
    return std::to_string(sqlite3_value_int64(v));
  case SQLITE_FLOAT:
    return std::to_string(sqlite3_value_double(v));
  case SQLITE_TEXT:
    return "\"" +
           string(reinterpret_cast<const char *>(sqlite3_value_text(v))) + "\"";
  case SQLITE_BLOB:
    return "<blob(" + std::to_string(sqlite3_value_bytes(v)) + ")>";
  default:
    return "NULL";
  }
}

static const char *op2s(int op) {
  switch (op) {
  case SQLITE_INSERT:
    return "INSERT";
  case SQLITE_UPDATE:
    return "UPDATE";
  case SQLITE_DELETE:
    return "DELETE";
  }
  return "?";
}

static const char *cf2s(int c) {
  switch (c) {
  case SQLITE_CHANGESET_DATA:
    return "DATA";
  case SQLITE_CHANGESET_NOTFOUND:
    return "NOTFOUND";
  case SQLITE_CHANGESET_CONFLICT:
    return "CONFLICT";
  case SQLITE_CHANGESET_CONSTRAINT:
    return "CONSTRAINT";
  case SQLITE_CHANGESET_FOREIGN_KEY:
    return "FOREIGN_KEY";
  }
  return "?";
}

static vector<string> &tableCols(Ctx *ctx, const string &table) {
  auto it = ctx->colsCache.find(table);
  if (it != ctx->colsCache.end())
    return it->second;
  vector<string> out;
  string sql = "PRAGMA table_info(\"" + table + "\")";
  sqlite3_stmt *st = nullptr;
  if (sqlite3_prepare_v2(ctx->db, sql.c_str(), -1, &st, nullptr) == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW)
      out.emplace_back(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
  }
  sqlite3_finalize(st);
  return ctx->colsCache[table] = out;
}

static void printRowDiff(Ctx *ctx, sqlite3_changeset_iter *it, const char *zTab,
                         int nCol) {
  auto &names = tableCols(ctx, zTab ? zTab : string{});
  unsigned char *abPK = nullptr;
  (void)sqlite3changeset_pk(it, &abPK);

  printf("\n  %-2s %-20s | %-18s | %-18s | %-18s\n", "pk", "column", "DB",
         "OLD", "NEW");
  printf("  %s\n", std::string(64, '-').c_str());
  for (int i = 0; i < nCol; ++i) {
    sqlite3_value *vDB = nullptr, *vOld = nullptr, *vNew = nullptr;
    (void)sqlite3changeset_conflict(it, i, &vDB);
    (void)sqlite3changeset_old(it, i, &vOld);
    (void)sqlite3changeset_new(it, i, &vNew);
    string col =
        (i < (int)names.size() ? names[i] : ("col" + std::to_string(i)));
    printf("  %-2s %-20s | %-18s | %-18s | %-18s\n",
           (abPK && abPK[i]) ? "*" : "", col.c_str(), v2s(vDB).c_str(),
           v2s(vOld).c_str(), v2s(vNew).c_str());
  }
}

static int filterAll(void *, const char *) { return 1; }

static int conflictHandler(void *pCtx, int eConflict,
                           sqlite3_changeset_iter *it) {
  auto *ctx = static_cast<Ctx *>(pCtx);
  const char *zTab = nullptr;
  int nCol = 0, op = 0, indirect = 0;
  sqlite3changeset_op(it, &zTab, &nCol, &op, &indirect);

  printf("\n=== Conflict: %s | %s on %s (indirect:%d) ===\n", cf2s(eConflict),
         op2s(op), zTab ? zTab : "?", indirect);
  printRowDiff(ctx, it, zTab ? zTab : "", nCol);

  bool canReplace = (eConflict == SQLITE_CHANGESET_DATA ||
                     eConflict == SQLITE_CHANGESET_CONFLICT);
  string prompt = string("Choose: [o]mit") + (canReplace ? ", [r]eplace" : "") +
                  ", [a]bort > ";
  for (;;) {
    std::fputs(prompt.c_str(), stdout);
    std::fflush(stdout);

    int c = std::getchar();
    if (c == '\n' || c == '\r')
      continue;

    int d;
    while ((d = std::getchar()) != '\n' && d != EOF) { /* discard */
    }

    c = std::tolower(static_cast<unsigned char>(c));
    if (c == 'o')
      return SQLITE_CHANGESET_OMIT;
    if (canReplace && c == 'r')
      return SQLITE_CHANGESET_REPLACE;
    if (c == 'a')
      return SQLITE_CHANGESET_ABORT;

    std::puts("Invalid choice.");
  }
}

static vector<unsigned char> readFile(const string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot open: " + path);
  f.seekg(0, std::ios::end);
  std::streamsize n = f.tellg();
  f.seekg(0, std::ios::beg);
  vector<unsigned char> buf((size_t)n);
  if (n > 0)
    f.read(reinterpret_cast<char *>(buf.data()), n);
  return buf;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <db.sqlite> <changeset.bin> [--dry-run]\n",
                 argv[0]);
    return 2;
  }
  const string dbPath = argv[1], csPath = argv[2];
  bool dry = (argc >= 4 && string(argv[3]) == "--dry-run");

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) !=
      SQLITE_OK) {
    std::fprintf(stderr, "open db: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  char *err = nullptr;
  sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &err);
  if (err) {
    std::fprintf(stderr, "pragma: %s\n", err);
    sqlite3_free(err);
  }

  vector<unsigned char> cs;
  try {
    cs = readFile(csPath);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    sqlite3_close(db);
    return 1;
  }

  Ctx ctx{db, dry, {}};

  int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    std::fprintf(stderr, "BEGIN: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  rc = sqlite3changeset_apply(db, static_cast<int>(cs.size()),
                              cs.empty() ? nullptr : cs.data(), filterAll,
                              conflictHandler, &ctx);

  if (rc == SQLITE_OK && !dry) {
    rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
      std::fprintf(stderr, "COMMIT: %s\n", sqlite3_errmsg(db));
      sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
  } else {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
  }

  if (rc != SQLITE_OK)
    std::fprintf(stderr, "apply: rc=%d (%s)\n", rc, sqlite3_errstr(rc));
  else
    std::fprintf(stdout, "%s\n",
                 dry ? "dry-run complete (rolled back)" : "changeset applied");

  sqlite3_close(db);
  return rc == SQLITE_OK ? 0 : 1;
}
