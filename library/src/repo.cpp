/******************************************************************************
* Copyright (c) 2016 - present Advanced Micro Devices, Inc. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/

#include <assert.h>
#include <iostream>
#include <vector>

#include "logging.h"
#include "plan.h"
#include "repo.h"
#include "rocfft.h"

// Implementation of Class Repo

std::mutex        Repo::mtx;
std::atomic<bool> Repo::repoDestroyed(false);

rocfft_status Repo::CreatePlan(rocfft_plan plan)
{
    std::lock_guard<std::mutex> lck(mtx);
    if(repoDestroyed)
        return rocfft_status_failure;

    Repo& repo = Repo::GetRepo();

    // see if the repo has already stored the plan or not
    auto it = repo.planUnique.find(*plan);
    if(it == repo.planUnique.end()) // if not found
    {
        auto rootPlan = TreeNode::CreateNode();

        rootPlan->dimension = plan->rank;
        rootPlan->batch     = plan->batch;
        for(size_t i = 0; i < plan->rank; i++)
        {
            rootPlan->length.push_back(plan->lengths[i]);

            rootPlan->inStride.push_back(plan->desc.inStrides[i]);
            rootPlan->outStride.push_back(plan->desc.outStrides[i]);
        }
        rootPlan->iDist = plan->desc.inDist;
        rootPlan->oDist = plan->desc.outDist;

        rootPlan->placement = plan->placement;
        rootPlan->precision = plan->precision;
        if((plan->transformType == rocfft_transform_type_complex_forward)
           || (plan->transformType == rocfft_transform_type_real_forward))
            rootPlan->direction = -1;
        else
            rootPlan->direction = 1;

        rootPlan->inArrayType  = plan->desc.inArrayType;
        rootPlan->outArrayType = plan->desc.outArrayType;

        ExecPlan execPlan;
        execPlan.rootPlan = std::move(rootPlan);
        ProcessNode(execPlan); // TODO: more descriptions are needed
        if(LOG_TRACE_ENABLED())
            PrintNode(*LogSingleton::GetInstance().GetTraceOS(), execPlan);

        if(!PlanPowX(execPlan)) // PlanPowX enqueues the GPU kernels by function
        {
            return rocfft_status_failure;
        }

        // pointers but does not execute kernels

        // add this plan into member planUnique (type of map)
        repo.planUnique[*plan] = std::make_pair(execPlan, 1);
        // add this plan into member execLookup (type of map)
        repo.execLookup[plan] = execPlan;
    }
    else // find the stored plan
    {
        repo.execLookup[plan]
            = it->second.first; // retrieve this plan and put it into member execLookup
        it->second.second++;
    }

    return rocfft_status_success;
}
// According to input plan, return the corresponding execPlan
void Repo::GetPlan(rocfft_plan plan, ExecPlan& execPlan)
{
    std::lock_guard<std::mutex> lck(mtx);
    if(repoDestroyed)
        return;

    Repo& repo = Repo::GetRepo();
    if(repo.execLookup.find(plan) != repo.execLookup.end())
        execPlan = repo.execLookup[plan];
}

// Remove the plan from Repo and release its ExecPlan resources if it is the last reference
void Repo::DeletePlan(rocfft_plan plan)
{
    std::lock_guard<std::mutex> lck(mtx);
    if(repoDestroyed)
        return;

    Repo& repo = Repo::GetRepo();
    auto  it   = repo.execLookup.find(plan);
    if(it != repo.execLookup.end())
    {
        repo.execLookup.erase(it);
    }

    auto it_u = repo.planUnique.find(*plan);
    if(it_u != repo.planUnique.end())
    {
        it_u->second.second--;
        if(it_u->second.second <= 0)
        {
            repo.planUnique.erase(it_u);
        }
    }
}

size_t Repo::GetUniquePlanCount()
{
    std::lock_guard<std::mutex> lck(mtx);
    if(repoDestroyed)
        return 0;

    Repo& repo = Repo::GetRepo();
    return repo.planUnique.size();
}

size_t Repo::GetTotalPlanCount()
{
    std::lock_guard<std::mutex> lck(mtx);
    if(repoDestroyed)
        return 0;

    Repo& repo = Repo::GetRepo();
    return repo.execLookup.size();
}
