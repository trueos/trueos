#ifndef PKG_REPOS_H
#define PKG_REPOS_H
extern struct pkg_repo_ops pkg_repo_binary_ops;
struct pkg_repo_ops* repos_ops[] = {
&pkg_repo_binary_ops,
NULL
};
#endif /* PKG_REPOS_H */
