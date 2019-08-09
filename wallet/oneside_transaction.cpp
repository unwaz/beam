// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "oneside_transaction.h"
#include "base_tx_builder.h"

using namespace std;

namespace beam {
    namespace wallet {

        BaseTransaction::Ptr OneSideTransaction::Create(INegotiatorGateway& gateway
            , IWalletDB::Ptr walletDB
            , IPrivateKeyKeeper::Ptr keyKeeper
            , const TxID& txID)
        {
            return BaseTransaction::Ptr(new OneSideTransaction(gateway, walletDB, keyKeeper, txID));

        }

        OneSideTransaction::OneSideTransaction(INegotiatorGateway& gateway
            , IWalletDB::Ptr walletDB
            , IPrivateKeyKeeper::Ptr keyKeeper
            , const TxID& txID)
            : BaseTransaction(gateway, walletDB, keyKeeper, txID)
        {

        }

        TxType OneSideTransaction::GetType() const
        {
            return TxType::OneSide;
        }

        void OneSideTransaction::UpdateImpl()
        {
            bool isSender = GetMandatoryParameter<bool>(TxParameterID::IsSender);

            AmountList amountList;
            if (!GetParameter(TxParameterID::AmountList, amountList))
            {
                amountList = AmountList{ GetMandatoryParameter<Amount>(TxParameterID::Amount) };
            }

            if (!isSender)
            {
                BaseTxBuilder receiverBuilder(*this, kDefaultSubTxID, amountList, 0);

                if (receiverBuilder.GetInitialTxParams())
                {
                    for (const auto& amount : receiverBuilder.GetAmountList())
                    {
                        receiverBuilder.GenerateNewCoin(amount, false);
                    }
                    receiverBuilder.GenerateOffset();
                }

                if (receiverBuilder.CreateOutputs())
                {
                    return;
                }

                receiverBuilder.CreateKernel();
                receiverBuilder.SignPartial();
                receiverBuilder.FinalizeSignature();
                UpdateOnNextTip();
                //m_Gateway.ConfirmOutputs(receiverBuilder.GetCoinIDs());
                return;
            }

            if (!m_TxBuilder)
            {
                m_TxBuilder = make_shared<BaseTxBuilder>(*this, kDefaultSubTxID, amountList, GetMandatoryParameter<Amount>(TxParameterID::Fee));
            }
            auto sharedBuilder = m_TxBuilder;
            BaseTxBuilder& builder = *sharedBuilder;

            if (!builder.GetPeerKernels())
            {
                OnFailed(TxFailureReason::MaxHeightIsUnacceptable, true);
            }

            // Check if we already have signed kernel
            if ((isSender && !builder.LoadKernel())
                || (!isSender && !builder.HasKernelID()))
            {
                // We don't need key keeper initialized to go on beyond this point
                if (!m_KeyKeeper)
                {
                    // public wallet
                    return;
                }

                if (!builder.GetInitialTxParams())
                {
                    LOG_INFO() << GetTxID() << (isSender ? " Sending " : " Receiving ")
                        << PrintableAmount(builder.GetAmount())
                        << " (fee: " << PrintableAmount(builder.GetFee()) << ")";

                    if (isSender)
                    {
                        Height maxResponseHeight = 0;
                        if (GetParameter(TxParameterID::PeerResponseHeight, maxResponseHeight))
                        {
                            LOG_INFO() << GetTxID() << " Max height for response: " << maxResponseHeight;
                        }

                        builder.SelectInputs();
                        builder.AddChange();
                    }

                    UpdateTxDescription(TxStatus::InProgress);

                    builder.GenerateOffset();
                }

                if (builder.CreateInputs())
                {
                    return;
                }

                if (builder.CreateOutputs())
                {
                    return;
                }

                uint64_t nAddrOwnID;
                if (!GetParameter(TxParameterID::MyAddressID, nAddrOwnID))
                {
                    WalletID wid;
                    if (GetParameter(TxParameterID::MyID, wid))
                    {
                        auto waddr = m_WalletDB->getAddress(wid);
                        if (waddr && waddr->m_OwnID)
                            SetParameter(TxParameterID::MyAddressID, waddr->m_OwnID);
                    }
                }

                builder.GenerateNonce();

                if (!builder.UpdateMaxHeight())
                {
                    OnFailed(TxFailureReason::MaxHeightIsUnacceptable, true);
                    return;
                }

                builder.CreateKernel();
                builder.SignPartial();

                if (!builder.GetPeerInputsAndOutputs())
                {
                    OnFailed(TxFailureReason::MaxHeightIsUnacceptable, true);
                    return;
                }

                if (IsInitiator() && !builder.IsPeerSignatureValid())
                {
                    OnFailed(TxFailureReason::InvalidPeerSignature, true);
                    return;
                }

                builder.FinalizeSignature();
            }

            uint8_t nRegistered = proto::TxStatus::Unspecified;
            if (!GetParameter(TxParameterID::TransactionRegistered, nRegistered))
            {
                if (CheckExpired())
                {
                    return;
                }

                // Construct transaction
                auto transaction = builder.CreateTransaction();

                // Verify final transaction
                TxBase::Context::Params pars;
                TxBase::Context ctx(pars);
                ctx.m_Height.m_Min = builder.GetMinHeight();
                if (!transaction->IsValid(ctx))
                {
                    OnFailed(TxFailureReason::InvalidTransaction, true);
                    return;
                }
                m_Gateway.register_tx(GetTxID(), transaction);
                //            SetState(State::Registration);
                return;
            }

            if (proto::TxStatus::Ok != nRegistered)
            {
                OnFailed(TxFailureReason::FailedToRegister, true);
                return;
            }

            Height hProof = 0;
            GetParameter(TxParameterID::KernelProofHeight, hProof);
            if (!hProof)
            {
                ConfirmKernel(builder.GetKernelID());
                return;
            }

            vector<Coin> modified = m_WalletDB->getCoinsByTx(GetTxID());
            for (auto& coin : modified)
            {
                bool bIn = (coin.m_createTxId == m_ID);
                bool bOut = (coin.m_spentTxId == m_ID);
                if (bIn || bOut)
                {
                    if (bIn)
                    {
                        coin.m_confirmHeight = std::min(coin.m_confirmHeight, hProof);
                        coin.m_maturity = hProof + Rules::get().Maturity.Std; // so far we don't use incubation for our created outputs
                    }
                    if (bOut)
                        coin.m_spentHeight = std::min(coin.m_spentHeight, hProof);
                }
            }

            GetWalletDB()->saveCoins(modified);

            CompleteTx();
        }
    }
}