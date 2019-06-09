// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Alessandro Tasora, Radu Serban
// =============================================================================

#include "chrono/solver/ChConstraintTwoGeneric.h"

namespace chrono {

// Register into the object factory, to enable run-time dynamic creation and persistence
CH_FACTORY_REGISTER(ChConstraintTwoGeneric)

ChConstraintTwoGeneric::ChConstraintTwoGeneric() {}

ChConstraintTwoGeneric::ChConstraintTwoGeneric(ChVariables* mvariables_a, ChVariables* mvariables_b) {
    SetVariables(mvariables_a, mvariables_b);
}

ChConstraintTwoGeneric::ChConstraintTwoGeneric(const ChConstraintTwoGeneric& other) : ChConstraintTwo(other) {
    Cq_a = other.Cq_a;
    Cq_b = other.Cq_b;
    Eq_a = other.Eq_a;
    Eq_b = other.Eq_b;
}

ChConstraintTwoGeneric& ChConstraintTwoGeneric::operator=(const ChConstraintTwoGeneric& other) {
    if (&other == this)
        return *this;

    // copy parent class data
    ChConstraintTwo::operator=(other);

    Cq_a = other.Cq_a;
    Cq_b = other.Cq_b;
    Eq_a = other.Eq_a;
    Eq_b = other.Eq_b;

    return *this;
}

void ChConstraintTwoGeneric::SetVariables(ChVariables* mvariables_a, ChVariables* mvariables_b) {
    if (!mvariables_a || !mvariables_b) {
        SetValid(false);
        return;
    }

    SetValid(true);
    variables_a = mvariables_a;
    variables_b = mvariables_b;

    if (variables_a->Get_ndof() > 0) {
        Cq_a.resize(1, variables_a->Get_ndof());
        Eq_a.resize(variables_a->Get_ndof(), 1);
        Cq_a.setZero();
    }

    if (variables_b->Get_ndof() > 0) {
        Cq_b.resize(1, variables_b->Get_ndof());
        Eq_b.resize(variables_b->Get_ndof(), 1);
        Cq_b.setZero();
    }
}

void ChConstraintTwoGeneric::Update_auxiliary() {
    // 1- Assuming jacobians are already computed, now compute
    //   the matrices [Eq_a]=[invM_a]*[Cq_a]' and [Eq_b]
    if (variables_a->IsActive() && variables_a->Get_ndof() > 0) {
        variables_a->Compute_invMb_v(Eq_a, Cq_a.transpose());
    }
    if (variables_b->IsActive() && variables_b->Get_ndof() > 0) {
        variables_b->Compute_invMb_v(Eq_b, Cq_b.transpose());
    }

    //// RADU
    //// (1) OK to use 'dot', even though Cq_a and Eq_a have different storage orders?
    ////     Otherwise, use something like:
    ////         g_i += (Cq_a * Eq_a)(0, 0);
    //// (2) How can I include the conditions in a single Eigen expression?
    ////     Option:
    ////         int a = (variables_a->IsActive() && variables_a->Get_ndof() > 0) ? 1 : 0;
    ////         int b = (variables_b->IsActive() && variables_b->Get_ndof() > 0) ? 1 : 0;
    ////         g_i = a * Cq_a.dot(Eq_a) + b * Cq_b.dot(Eq_b);
    ////     Is it worth it?

   // 2- Compute g_i = [Cq_i]*[invM_i]*[Cq_i]' + cfm_i
    g_i = 0;
    if (variables_a->IsActive() && variables_a->Get_ndof() > 0) {
        g_i += Cq_a.dot(Eq_a);
    }
    if (variables_b->IsActive() && variables_b->Get_ndof() > 0) {
        g_i += Cq_b.dot(Eq_b);
    }

    // 3- adds the constraint force mixing term (usually zero):
    if (cfm_i != 0)
        g_i += cfm_i;
}

double ChConstraintTwoGeneric::Compute_Cq_q() {
    double ret = 0;

    if (variables_a->IsActive()) {
        ret += Cq_a.dot(variables_a->Get_qb());
    }

    if (variables_b->IsActive()) {
        ret += Cq_b.dot(variables_b->Get_qb());
    }

    return ret;
}

void ChConstraintTwoGeneric::Increment_q(const double deltal) {
    if (variables_a->IsActive()) {
        variables_a->Get_qb() += Eq_a * deltal;
    }

    if (variables_b->IsActive()) {
        variables_b->Get_qb() += Eq_b * deltal;
    }
}

void ChConstraintTwoGeneric::MultiplyAndAdd(double& result, const ChVectorDynamic<double>& vect) const {
    if (variables_a->IsActive()) {
        result += Cq_a.dot(vect.segment(variables_a->GetOffset(), Cq_a.cols()));
    }

    if (variables_b->IsActive()) {
        result += Cq_b.dot(vect.segment(variables_b->GetOffset(), Cq_b.cols()));
    }
}

void ChConstraintTwoGeneric::MultiplyTandAdd(ChVectorDynamic<double>& result, double l) {
    if (variables_a->IsActive()) {
        result.segment(variables_a->GetOffset(), Cq_a.cols()) += Cq_a * l;
    }

    if (variables_b->IsActive()) {
        result.segment(variables_b->GetOffset(), Cq_b.cols()) += Cq_b * l;
    }
}

void ChConstraintTwoGeneric::Build_Cq(ChSparseMatrix& storage, int insrow) {
    if (variables_a->IsActive())
        storage.PasteMatrix(Cq_a, insrow, variables_a->GetOffset());
    if (variables_b->IsActive())
        storage.PasteMatrix(Cq_b, insrow, variables_b->GetOffset());
}

void ChConstraintTwoGeneric::Build_CqT(ChSparseMatrix& storage, int inscol) {
    if (variables_a->IsActive())
        storage.PasteTranspMatrix(Cq_a, variables_a->GetOffset(), inscol);
    if (variables_b->IsActive())
        storage.PasteTranspMatrix(Cq_b, variables_b->GetOffset(), inscol);
}

void ChConstraintTwoGeneric::ArchiveOUT(ChArchiveOut& marchive) {
    // version number
    marchive.VersionWrite<ChConstraintTwoGeneric>();

    // serialize the parent class data too
    ChConstraintTwo::ArchiveOUT(marchive);

    // serialize all member data:
    // NOTHING INTERESTING TO SERIALIZE (the Cq jacobians are not so
    // important to waste disk space.. they may be recomputed run-time,
    // and pointers to variables must be rebound in run-time.)
    // mstream << Cq_a;
    // mstream << Cq_b;
}

void ChConstraintTwoGeneric::ArchiveIN(ChArchiveIn& marchive) {
    // version number
    int version = marchive.VersionRead<ChConstraintTwoGeneric>();

    // deserialize the parent class data too
    ChConstraintTwo::ArchiveIN(marchive);

    // deserialize all member data:
    // NOTHING INTERESTING TO SERIALIZE (the Cq jacobians are not so
    // important to waste disk space.. they may be recomputed run-time,
    // and pointers to variables must be rebound in run-time.)
    // mstream << Cq_a;
    // mstream << Cq_b;
}


}  // end namespace chrono
