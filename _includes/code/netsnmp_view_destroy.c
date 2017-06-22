void
netsnmp_view_destroy(struct vacm_viewEntry **head, const char *viewName,
                      oid * viewSubtree, size_t viewSubtreeLen)
{
    struct vacm_viewEntry *vp, *lastvp = NULL;

    if ((*head) && !strcmp((*head)->viewName + 1, viewName)
        && (*head)->viewSubtreeLen == viewSubtreeLen
        && !memcmp((char *) (*head)->viewSubtree, (char *) viewSubtree,
                   viewSubtreeLen * sizeof(oid))) {
        vp = (*head);
        (*head) = (*head)->next;
    } else {
        for (vp = (*head); vp; vp = vp->next) {
            if (!strcmp(vp->viewName + 1, viewName)
                && vp->viewSubtreeLen == viewSubtreeLen
                && !memcmp((char *) vp->viewSubtree, (char *) viewSubtree,
                           viewSubtreeLen * sizeof(oid)))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}
