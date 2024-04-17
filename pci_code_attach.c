    /*
     * Find our PCI CONFIG registers.
     */
    cfg = (volatile uchar_t *) pciio_pio_addr
        (conn, 0,               /* device and (override) dev_info */
         PCIIO_SPACE_CFG,       /* select configuration addr space */
         0,                     /* from the start of space, */
         PCI_CFG_VEND_SPECIFIC, /* ... up to vendor specific stuff */
         &cmap,                 /* in case we needed a piomap */
         0);                    /* flag word */
    soft->ps_cfg = cfg;         /* save for later */
    soft->ps_cmap = cmap;
    printf("usbehci_attach: I can see my CFG regs at 0x%x\n", cfg);
    /*
     * Get a pointer to our DEVICE registers
     */
    regs = (usbehci_regs_t) pciio_pio_addr
        (conn, 0,               /* device and (override) dev_info */
         PCIIO_SPACE_WIN(0),    /* in my primary decode window, */
         0, sizeof(*regs),      /* base and size */
         &rmap,                 /* in case we needed a piomap */
         0);                    /* flag word */
    soft->ps_regs = regs;       /* save for later */
    soft->ps_rmap = rmap;
    printf("usbehci_attach: I can see my device regs at 0x%x\n", regs);

    soft->ps_intr = pciio_intr_alloc(conn, 0, PCIIO_INTR_LINE_A | PCIIO_INTR_LINE_B, vhdl);
    pciio_intr_connect(soft->ps_intr, usbehci_dma_intr, soft,(void *) 0);
    pciio_error_register(conn, usbehci_error_handler, soft);

    soft->ps_pollhead = phalloc(0);




static struct{
    uint32_t       device_id;
    uchar_t          *controller_description;
}uhci_descriptions[]={
    0x26888086, "Intel 631XESB/632XESB/3100 USB controller USB-1",
    0x26898086,"Intel 631XESB/632XESB/3100 USB controller USB-2",
    0x268a8086,"Intel 631XESB/632XESB/3100 USB controller USB-3",
    0x268b8086,"Intel 631XESB/632XESB/3100 USB controller USB-4",
    0x70208086,"Intel 82371SB (PIIX3) USB controller",
    0x71128086,"Intel 82371AB/EB (PIIX4) USB controller",
    0x24128086,"Intel 82801AA (ICH) USB controller",
    0x24228086,"Intel 82801AB (ICH0) USB controller",
    0x24428086,"Intel 82801BA/BAM (ICH2) USB controller USB-A",
    0x24448086,"Intel 82801BA/BAM (ICH2) USB controller USB-B",
    0x24828086,"Intel 82801CA/CAM (ICH3) USB controller USB-A",
    0x24848086,"Intel 82801CA/CAM (ICH3) USB controller USB-B",
    0x24878086,"Intel 82801CA/CAM (ICH3) USB controller USB-C",
    0x24c28086,"Intel 82801DB (ICH4) USB controller USB-A",
    0x24c48086,"Intel 82801DB (ICH4) USB controller USB-B",
    0x24c78086,"Intel 82801DB (ICH4) USB controller USB-C",
    0x24d28086,"Intel 82801EB (ICH5) USB controller USB-A",
    0x24d48086,"Intel 82801EB (ICH5) USB controller USB-B",
    0x24d78086,"Intel 82801EB (ICH5) USB controller USB-C",
    0x24de8086,"Intel 82801EB (ICH5) USB controller USB-D",
    0x26588086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-A",
    0x26598086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-B",
    0x265a8086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-C",
    0x265b8086,"Intel 82801FB/FR/FW/FRW (ICH6) USB controller USB-D",
    0x27c88086,"Intel 82801G (ICH7) USB controller USB-A",
    0x27c98086,"Intel 82801G (ICH7) USB controller USB-B",
    0x27ca8086,"Intel 82801G (ICH7) USB controller USB-C",
    0x27cb8086,"Intel 82801G (ICH7) USB controller USB-D",
    0x28308086,"Intel 82801H (ICH8) USB controller USB-A",
    0x28318086,"Intel 82801H (ICH8) USB controller USB-B",
    0x28328086,"Intel 82801H (ICH8) USB controller USB-C",
    0x28348086,"Intel 82801H (ICH8) USB controller USB-D",
    0x28358086,"Intel 82801H (ICH8) USB controller USB-E",
    0x29348086,"Intel 82801I (ICH9) USB controller",
    0x29358086,"Intel 82801I (ICH9) USB controller",
    0x29368086,"Intel 82801I (ICH9) USB controller",
    0x29378086,"Intel 82801I (ICH9) USB controller",
    0x29388086,"Intel 82801I (ICH9) USB controller",
    0x29398086,"Intel 82801I (ICH9) USB controller",
    0x3a348086,"Intel 82801JI (ICH10) USB controller USB-A",
    0x3a358086,"Intel 82801JI (ICH10) USB controller USB-B",
    0x3a368086,"Intel 82801JI (ICH10) USB controller USB-C",
    0x3a378086,"Intel 82801JI (ICH10) USB controller USB-D",
    0x3a388086,"Intel 82801JI (ICH10) USB controller USB-E",
    0x3a398086,"Intel 82801JI (ICH10) USB controller USB-F",
    0x719a8086,"Intel 82443MX USB controller",
    0x76028086,"Intel 82372FB/82468GX USB controller",
    0x30381106,"VIA VT6212 UHCI USB 1.0 controller",
    0 ,NULL
};
