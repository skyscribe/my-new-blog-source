struct vacm_viewEntry *
netsnmp_view_create(struct vacm_viewEntry **head, const char *viewName,
                     oid * viewSubtree, size_t viewSubtreeLen)
{
    struct vacm_viewEntry *vp, *lp, *op = NULL;
    int cmp, cmp2, glen;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen >= VACM_MAX_STRING)
        return NULL;
    vp = (struct vacm_viewEntry *) calloc(1,
                                          sizeof(struct vacm_viewEntry));
    if (vp == NULL)
        return NULL;
    vp->reserved =
        (struct vacm_viewEntry *) calloc(1, sizeof(struct vacm_viewEntry));
    if (vp->reserved == NULL) {
        free(vp);
        return NULL;
    }

    vp->viewName[0] = glen;
    strcpy(vp->viewName + 1, viewName);
    vp->viewSubtree[0] = viewSubtreeLen;
    memcpy(vp->viewSubtree + 1, viewSubtree, viewSubtreeLen * sizeof(oid));
    vp->viewSubtreeLen = viewSubtreeLen + 1;

    lp = *head;
    while (lp) {
        cmp = memcmp(lp->viewName, vp->viewName, glen + 1);
        cmp2 = snmp_oid_compare(lp->viewSubtree, lp->viewSubtreeLen,
                                vp->viewSubtree, vp->viewSubtreeLen);
        if (cmp == 0 && cmp2 > 0)
            break;
        if (cmp > 0)
            break;
        op = lp;
        lp = lp->next;
    }
    vp->next = lp;
    if (op)
        op->next = vp;
    else
        *head = vp;
    return vp;
}
