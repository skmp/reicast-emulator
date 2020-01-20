
// shared between virt_x86 and virt_arm32

void LoadReg(eReg rd, u32 regn, ConditionCode cc = CC_AL)
{
    LDR(rd, r8, (u8*)&arm_reg[regn].I - (u8*)&arm_reg[0].I, Offset, cc);
}
void StoreReg(eReg rd, u32 regn, ConditionCode cc = CC_AL)
{
    STR(rd, r8, (u8*)&arm_reg[regn].I - (u8*)&arm_reg[0].I, Offset, cc);
}

void LoadFlags()
{
    //Load flags
    LoadReg(r0, RN_PSR_FLAGS);
    //move them to flags register
    MSR(0, 8, r0);
}

void StoreFlags()
{
    //get results from flags register
    MRS(r1, 0);
    //Store flags
    StoreReg(r1, RN_PSR_FLAGS);
}

void mov(ARM::eReg regd, ARM::eReg regn)
{
    MOV(regd, regn);
}

void add(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
{
    ADD(regd, regn, regm);
}

void sub(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
{
    SUB(regd, regn, regm);
}

void add(ARM::eReg regd, ARM::eReg regn, s32 imm)
{
    if (imm >= 0)
        ADD(regd, regn, imm);
    else
        SUB(regd, regn, -imm);
}

void lsl(ARM::eReg regd, ARM::eReg regn, u32 imm)
{
    LSL(regd, regn, imm);
}

void bic(ARM::eReg regd, ARM::eReg regn, u32 imm)
{
    BIC(regd, regn, imm);
}
